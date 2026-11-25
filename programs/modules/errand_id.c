// SPDX-License-Identifier: GPL-2.0
/*
 * errand_id -- registers a guest process with the VMM so the vmcall exit picks that process's
 * capability table by its CR3 (w11_vmcall_identity).
 *
 * The registration is a vmcall from ring 0 (this module): under VMCALL_IDENTITY it takes the
 * slow path to the VMM's vmcall_handler, which binds the root to the process component. The
 * ABI is errand_id_abi.h, a byte-identical copy of the Composite tree's.
 *
 * Roots are exact guest-physical addresses (DECISIONS A8): __pa(pgd), and under PTI also the
 * user root, pgd + 4 KiB, which is what CR3 holds while the process runs in ring 3.
 *
 * Phase-1 gap, on purpose: nothing stops Linux from freeing a registered PGD and reusing it
 * for another process (exit_mm runs before the file is released). ipc_perf unregisters
 * explicitly before it exits; Phase 2's lock and revoke-on-write close the gap.
 *
 * Phase 2 (REGISTER_LOCKED): the module walks current's user page tables, pre-sets the accessed
 * bit on every present entry and the dirty bit on every present writable leaf (unless
 * ERRAND_ID_LOCK_NO_PRESET), collects the guest-physical address of every page-table page (both
 * PGD pages under PTI), flushes the TLB (__flush_tlb_all: the guest has one vCPU), and sends the list in one registration page with a
 * ring-0 vmcall. The VMM locks the pages read-only in the EPT, then registers.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/uaccess.h>
#include <asm/cpufeature.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/pgtable.h>
#include <linux/gfp.h>
#include <asm/tlbflush.h>
#include <asm/msr.h>
#include <linux/io.h>

#include "errand_id_abi.h"

/* A ring-0 vmcall: rax = op, rbx = arg; the VMM answers in rax and resumes at RIP + 3. */
static long
errand_id_vmcall(unsigned long op, unsigned long arg)
{
	unsigned long rax = op;

	asm volatile("vmcall" : "+a"(rax) : "b"(arg) : "memory");
	return (long)rax;
}

static int errand_id_roots(unsigned long roots[2]);

/* A ring-0 vmcall with three arguments (REGISTER_LOCKED). */
static long
errand_id_vmcall3(unsigned long op, unsigned long a, unsigned long b, unsigned long c)
{
	unsigned long rax = op;

	asm volatile("vmcall" : "+a"(rax) : "b"(a), "c"(b), "d"(c) : "memory");
	return (long)rax;
}

static struct errand_id_lock_page *lock_page; /* the registration page, one per module */
static struct errand_id_reg_stat   last_reg;

static int
lock_add(unsigned long gpa)
{
	if (lock_page->count >= ERRAND_ID_LOCK_MAX) return -ENOSPC;
	lock_page->gpa[lock_page->count++] = gpa;
	return 0;
}

/*
 * Walk current's user half: every page-table page goes into lock_page; with preset, accessed on
 * every present entry, dirty on every present writable leaf. A huge PUD or PMD refuses (-EINVAL).
 * Called with the mmap lock held for writing.
 */
static int
lock_walk(struct mm_struct *mm, int preset)
{
	unsigned long gi, ui, mi, ti, a;
	int           ret;

	if (pgtable_l5_enabled()) return -EINVAL;
	if ((ret = lock_add(__pa(mm->pgd)))) return ret;
	if (boot_cpu_has(X86_FEATURE_PTI) && (ret = lock_add(__pa(mm->pgd) + PAGE_SIZE))) return ret;

	for (gi = 0; gi <= pgd_index(TASK_SIZE_MAX - 1); gi++) {
		pgd_t *pgd;
		p4d_t *p4d;
		pud_t *pud;

		a   = gi << PGDIR_SHIFT;
		pgd = pgd_offset(mm, a);
		/* 4-level paging: the P4D is folded into the PGD, and pgd_none()/pgd_bad() are constant 0 */
		p4d = p4d_offset(pgd, a);
		if (p4d_none(*p4d) || p4d_bad(*p4d)) continue;
		if (preset && !(p4d_val(*p4d) & _PAGE_ACCESSED)) set_p4d(p4d, __p4d(p4d_val(*p4d) | _PAGE_ACCESSED));
		pud = pud_offset(p4d, a);
		if ((ret = lock_add(__pa(pud)))) return ret;
		for (ui = 0; ui < PTRS_PER_PUD; ui++, pud++) {
			unsigned long a2 = a + (ui << PUD_SHIFT);
			pmd_t        *pmd;

			if (pud_none(*pud)) continue;
			if (pud_large(*pud) || pud_bad(*pud)) return -EINVAL;
			if (preset && !(pud_val(*pud) & _PAGE_ACCESSED)) set_pud(pud, __pud(pud_val(*pud) | _PAGE_ACCESSED));
			pmd = pmd_offset(pud, a2);
			if ((ret = lock_add(__pa(pmd)))) return ret;
			for (mi = 0; mi < PTRS_PER_PMD; mi++, pmd++) {
				unsigned long a3 = a2 + (mi << PMD_SHIFT);
				pte_t        *pte;

				if (pmd_none(*pmd)) continue;
				if (pmd_large(*pmd) || pmd_bad(*pmd)) return -EINVAL;
				if (preset && !(pmd_val(*pmd) & _PAGE_ACCESSED)) set_pmd(pmd, __pmd(pmd_val(*pmd) | _PAGE_ACCESSED));
				pte = pte_offset_kernel(pmd, a3);
				if ((ret = lock_add(__pa(pte)))) return ret;
				if (!preset) continue;
				for (ti = 0; ti < PTRS_PER_PTE; ti++, pte++) {
					pte_t v = *pte;

					if (!pte_present(v)) continue;
					v = pte_mkyoung(v);
					if (pte_write(v)) v = pte_mkdirty(v);
					if (pte_val(v) != pte_val(*pte)) set_pte(pte, v);
				}
			}
		}
	}
	return 0;
}

static long
errand_id_register_locked(unsigned long flags)
{
	struct mm_struct *mm = current->mm;
	unsigned long     roots[2], t0, t1;
	long              ret;
	int               n;

	n = errand_id_roots(roots);
	if (!n || !lock_page) return -EINVAL;
	mmap_write_lock(mm);
	lock_page->count = 0;
	ret = lock_walk(mm, !(flags & ERRAND_ID_LOCK_NO_PRESET));
	preempt_disable(); __flush_tlb_all(); preempt_enable(); /* flush_tlb_mm is not available to modules; one vCPU */
	mmap_write_unlock(mm);
	if (ret) {
		pr_info("errand_id: register-locked pid %d: walk failed %ld (%lu pages)\n", current->pid, ret, lock_page->count);
		return ret == -ENOSPC ? -ENOMEM : ret;
	}
	t0  = rdtsc_ordered();
	ret = errand_id_vmcall3(ERRAND_ID_OP_REGISTER_LOCKED, roots[0], n > 1 ? roots[1] : 0, __pa(lock_page));
	t1  = rdtsc_ordered();
	last_reg.pages  = lock_page->count;
	last_reg.cycles = t1 - t0;
	last_reg.ret    = ret;
	pr_info("errand_id: register-locked root %#lx (pid %d): %lu page-table pages%s, %lu cycles: %ld\n", roots[0],
	        current->pid, lock_page->count, (flags & ERRAND_ID_LOCK_NO_PRESET) ? " (no preset)" : "",
	        t1 - t0, ret);
	return ret;
}

/*
 * T4: rewrite current's leaf entry for user address `va` to point at a module page holding a copy
 * of the same bytes (so the process keeps running), as a compromised kernel would. On a locked
 * process the set_pte is the first write to a locked page. `va | 1` puts the original entry back.
 */
static pte_t        t4_saved, *t4_ptep;
static struct page *t4_page;

static long
errand_id_t4(unsigned long arg)
{
	struct mm_struct *mm = current->mm;
	unsigned long     va = arg & PAGE_MASK;
	pgd_t            *pgd;
	p4d_t            *p4d;
	pud_t            *pud;
	pmd_t            *pmd;
	pte_t            *pte;

	if (arg & 1) {
		if (!t4_ptep) return -EINVAL;
		set_pte(t4_ptep, t4_saved);
		preempt_disable(); __flush_tlb_all(); preempt_enable(); /* flush_tlb_mm is not available to modules; one vCPU */
		t4_ptep = NULL;
		pr_info("errand_id: T4 restored the entry\n");
		return 0;
	}
	pgd = pgd_offset(mm, va);
	if (pgd_none(*pgd)) return -EFAULT;
	p4d = p4d_offset(pgd, va);
	pud = pud_offset(p4d, va);
	if (pud_none(*pud) || pud_large(*pud)) return -EFAULT;
	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_large(*pmd)) return -EFAULT;
	pte = pte_offset_kernel(pmd, va);
	if (!pte_present(*pte)) return -EFAULT;
	if (!t4_page && !(t4_page = alloc_page(GFP_KERNEL))) return -ENOMEM;
	memcpy(page_address(t4_page), page_address(pte_page(*pte)), PAGE_SIZE);
	t4_saved = *pte;
	t4_ptep  = pte;
	pr_info("errand_id: T4 rewriting the entry for %#lx (pid %d) to module page %#lx\n", va, current->pid,
	        (unsigned long)page_to_phys(t4_page));
	set_pte(pte, pfn_pte(page_to_pfn(t4_page), pte_pgprot(t4_saved)));
	preempt_disable(); __flush_tlb_all(); preempt_enable(); /* flush_tlb_mm is not available to modules; one vCPU */
	return 0;
}

static int
errand_id_roots(unsigned long roots[2])
{
	struct mm_struct *mm = current->mm;

	if (!mm) return 0;
	roots[0] = __pa(mm->pgd);
	if (!boot_cpu_has(X86_FEATURE_PTI)) return 1;
	roots[1] = roots[0] + PAGE_SIZE; /* kernel_to_user_pgdp() */

	return 2;
}

static long
errand_id_register(unsigned long op)
{
	unsigned long roots[2];
	int n = errand_id_roots(roots), i;
	long ret = 0;

	if (!n) return -EINVAL;
	for (i = 0; i < n; i++) {
		ret = errand_id_vmcall(op, roots[i]);
		pr_info("errand_id: %s root %#lx (pid %d): %ld\n",
			op == ERRAND_ID_OP_REGISTER ? "register" : "unregister", roots[i], current->pid, ret);
		/* a locked registration's second root goes with the first (pt_lock.c) */
		if (op == ERRAND_ID_OP_UNREGISTER && i > 0 && ret == -ERRAND_ID_ENOENT) ret = 0;
		if (ret) break;
	}

	return ret;
}

static long
errand_id_kernel_vmcall(unsigned long uarg)
{
	struct errand_id_kvmcall k;
	unsigned long i;

	if (copy_from_user(&k, (void __user *)uarg, sizeof(k))) return -EFAULT;
	k.n_enosys = k.n_other = 0;
	for (i = 0; i < k.count; i++) {
		k.last_rax = (unsigned long)errand_id_vmcall(k.rax, 0);
		if ((long)k.last_rax == -ERRAND_ID_ENOSYS) k.n_enosys++;
		else k.n_other++;
		if ((i & 0xfff) == 0) cond_resched();
	}
	if (copy_to_user((void __user *)uarg, &k, sizeof(k))) return -EFAULT;

	return 0;
}

static long
errand_id_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	unsigned long roots[2];

	switch ((unsigned long)cmd) {
	case ERRAND_ID_IOC_REGISTER:
		return errand_id_register(ERRAND_ID_OP_REGISTER);
	case ERRAND_ID_IOC_UNREGISTER:
		return errand_id_register(ERRAND_ID_OP_UNREGISTER);
	case ERRAND_ID_IOC_ROOT:
		if (!errand_id_roots(roots)) return -EINVAL;
		return put_user(roots[0], (unsigned long __user *)arg);
	case ERRAND_ID_IOC_KERNEL_VMCALL:
		return errand_id_kernel_vmcall(arg);
	case ERRAND_ID_IOC_COUNTERS:
		return errand_id_vmcall(ERRAND_ID_OP_COUNTERS, 0);
	case ERRAND_ID_IOC_REGISTER_LOCKED:
		return errand_id_register_locked(arg);
	case ERRAND_ID_IOC_DUMP:
		return errand_id_vmcall(ERRAND_ID_OP_DUMP, 0);
	case ERRAND_ID_IOC_T4_REWRITE:
		return errand_id_t4(arg);
	case ERRAND_ID_IOC_LAST_REG:
		return copy_to_user((void __user *)arg, &last_reg, sizeof(last_reg)) ? -EFAULT : 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations errand_id_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = errand_id_ioctl,
	.llseek         = noop_llseek,
};

static struct miscdevice errand_id_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "errand_id",
	.fops  = &errand_id_fops,
	.mode  = 0666,
};

static int __init
errand_id_init(void)
{
	int ret;

	lock_page = (struct errand_id_lock_page *)get_zeroed_page(GFP_KERNEL);
	if (!lock_page) return -ENOMEM;
	ret = misc_register(&errand_id_dev);

	if (ret) {
		pr_err("errand_id: misc_register failed (%d)\n", ret);
		return ret;
	}
	pr_info("errand_id: /dev/errand_id ready (PTI %s)\n", boot_cpu_has(X86_FEATURE_PTI) ? "on" : "off");
	return 0;
}

static void __exit
errand_id_exit(void)
{
	misc_deregister(&errand_id_dev);
	free_page((unsigned long)lock_page);
}

module_init(errand_id_init);
module_exit(errand_id_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("w11: per-process identity registration for the vmcall exit");
