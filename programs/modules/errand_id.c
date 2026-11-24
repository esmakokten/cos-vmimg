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
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/uaccess.h>
#include <asm/cpufeature.h>
#include <asm/page.h>

#include "errand_id_abi.h"

/* A ring-0 vmcall: rax = op, rbx = arg; the VMM answers in rax and resumes at RIP + 3. */
static long
errand_id_vmcall(unsigned long op, unsigned long arg)
{
	unsigned long rax = op;

	asm volatile("vmcall" : "+a"(rax) : "b"(arg) : "memory");
	return (long)rax;
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
	int ret = misc_register(&errand_id_dev);

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
}

module_init(errand_id_init);
module_exit(errand_id_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("w11: per-process identity registration for the vmcall exit");
