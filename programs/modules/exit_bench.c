// SPDX-License-Identifier: GPL-2.0
/*
 * exit_bench -- guest-kernel side of the w3_exit_anatomy exit ladder.
 *
 * One ioctl runs one rung N times and hands the raw per-sample cycle counts
 * back to userspace. Percentiles are computed there; the kernel does no
 * statistics, so no summarising decision is buried where it cannot be audited.
 *
 * Two things this module does that its predecessor (helpers mesurement-module)
 * did not, both of which change the numbers:
 *
 *  - it runs each chunk with interrupts disabled and preemption off, so a
 *    timer tick cannot land inside a sample. The predecessor's max of 2.2M
 *    cycles against a p50 of 3,520 is exactly that: host and guest
 *    interference, recorded as if it were exit cost.
 *
 *  - it re-enables interrupts between chunks. A 200k-iteration run at 3.6k
 *    cycles is ~0.34s; holding interrupts off for that long earns an RCU stall
 *    and a guest whose clock has drifted.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <asm/msr.h>

#include "exitbench_abi.h"
#include "exitbench_rungs.h"

#define EB_DEFAULT_CHUNK 1024
#define EB_MAX_SAMPLES   (8u << 20)

static bool allow_hlt;
module_param(allow_hlt, bool, 0644);
MODULE_PARM_DESC(allow_hlt,
	"enable RUNG_L5_HLT. Off by default: without a pending interrupt the vCPU blocks and the sample measures a scheduler round trip, not an exit.");

static u64 fallback_mmio_gpa(void)
{
	u64 top = (u64)totalram_pages() << PAGE_SHIFT;

	/* Round up past the top of RAM to the next GiB. Below the PCI hole and
	 * above every memslot, so nothing claims it and KVM must punt to
	 * userspace. The host harness normally passes the address explicitly;
	 * this is only the fallback, and run.sh checks the value the module
	 * reports back against the one it intended. */
	return ALIGN(top + (1ULL << 30), 1ULL << 30);
}

static noinline void eb_run_chunk(u32 rung, u32 flags, u64 count, u64 *out,
				  volatile unsigned int *mmio,
				  volatile unsigned char *pollute, u64 deadline)
{
	u64 i, t0, t1;

	for (i = 0; i < count; i++) {
		if (flags & EXITBENCH_F_COLD)
			eb_pollute(pollute, i);

		t0 = eb_tsc_begin();
		switch (rung) {
		case RUNG_L0_RDTSC:        eb_rung_l0_rdtsc();             break;
		case RUNG_L1_FASTPATH_MSR: eb_rung_l1_fastpath_msr(deadline); break;
		case RUNG_L2_VMCALL:       eb_rung_l2_vmcall();            break;
		case RUNG_L3_CPUID:        eb_rung_l3_cpuid();             break;
		case RUNG_L4_RDMSR:        eb_rung_l4_rdmsr();             break;
		case RUNG_L5_HLT:          eb_rung_l5_hlt();               break;
		case RUNG_L6_PIO:          eb_rung_l6_pio();               break;
		case RUNG_L7_MMIO:         eb_rung_l7_mmio(mmio);          break;
		}
		t1 = eb_tsc_end();
		out[i] = t1 - t0;
	}
}

static long eb_run(struct exitbench_req *req)
{
	u64 *samples = NULL;
	volatile unsigned int *mmio = NULL;
	volatile unsigned char *pollute = NULL;
	u64 done = 0, chunk, gpa = 0, saved_deadline = 0, deadline = 0;
	int start_cpu;
	long ret = 0;

	if (req->rung >= RUNG_MAX)
		return -EINVAL;
	if (!req->n || req->n > EB_MAX_SAMPLES)
		return -EINVAL;
	if (req->rung == RUNG_L5_HLT && !allow_hlt)
		return -EPERM;

	chunk = req->chunk ? req->chunk : EB_DEFAULT_CHUNK;
	if (chunk > req->n)
		chunk = req->n;

	samples = vmalloc(req->n * sizeof(u64));
	if (!samples)
		return -ENOMEM;

	if (req->flags & EXITBENCH_F_COLD) {
		pollute = vmalloc(EB_POLLUTE_BYTES);
		if (!pollute) { ret = -ENOMEM; goto out; }
		memset((void *)pollute, 0, EB_POLLUTE_BYTES);
	}

	if (req->rung == RUNG_L7_MMIO) {
		gpa = req->mmio_gpa ? req->mmio_gpa : fallback_mmio_gpa();
		mmio = ioremap(gpa, PAGE_SIZE);
		if (!mmio) { ret = -ENOMEM; goto out; }
		req->mmio_gpa_used = gpa;
	}

	if (req->rung == RUNG_L1_FASTPATH_MSR) {
		/* The deadline has to clear two hurdles at once, and the obvious
		 * choice fails the second.
		 *
		 * Far enough ahead that it never fires during the run -- and
		 * near enough that KVM can express it in the VMX preemption
		 * timer. That timer is a 32-bit countdown in units of
		 * 2^IA32_VMX_MISC[4:0] TSC ticks, so its reach is ~1.4e11
		 * ticks. A 2^40 (1.1e12) deadline is out of range:
		 * vmx_set_hv_timer returns -ERANGE and KVM falls back to
		 * start_sw_tscdeadline(), an hrtimer start per iteration. That
		 * is not the fast path -- it is slower than VMCALL, which is
		 * exactly what the first run of this rung measured.
		 *
		 * 2^31 ticks is ~1s at 2.1 GHz: comfortably inside the timer's
		 * range and far beyond any single sample.
		 *
		 * Restore the guest's own deadline afterwards, or it loses its
		 * tick for the rest of the boot. */
		rdmsrl(MSR_IA32_TSC_DEADLINE, saved_deadline);
		deadline = rdtsc() + (1ULL << 31);
	}

	/* Every sample must come from one core, or TSC skew turns into apparent
	 * exit cost. The task is already pinned by the userspace driver
	 * (sched_setaffinity), and interrupts are off inside each chunk, so it
	 * cannot migrate mid-sample. Deliberately NOT get_cpu(): that disables
	 * preemption for the whole run, and cond_resched() below would then be
	 * a schedule-while-atomic bug. Instead, check afterwards that the core
	 * did not change -- a silent migration would corrupt every number and
	 * leave no trace. */
	start_cpu = raw_smp_processor_id();

	while (done < req->n) {
		u64 this = min(chunk, req->n - done);
		unsigned long irqflags;

		if (req->rung == RUNG_L5_HLT) {
			/* HLT needs interrupts on to be woken at all. */
			eb_run_chunk(req->rung, req->flags, this,
				     samples + done, mmio, pollute, deadline);
		} else {
			local_irq_save(irqflags);
			eb_run_chunk(req->rung, req->flags, this,
				     samples + done, mmio, pollute, deadline);
			local_irq_restore(irqflags);
		}
		done += this;
		/* Let the guest breathe: timers, RCU, the watchdog. */
		cond_resched();
	}

	/* Restore before the migration check: bailing out with the guest's timer
	 * deadline parked 2^40 cycles in the future costs it its tick for the
	 * rest of the boot, and the run that reported the migration would be
	 * blamed for everything measured afterwards. */
	if (req->rung == RUNG_L1_FASTPATH_MSR)
		wrmsrl(MSR_IA32_TSC_DEADLINE, saved_deadline);

	if (raw_smp_processor_id() != start_cpu) {
		pr_warn("exit_bench: migrated %d -> %d mid-run; samples discarded\n",
			start_cpu, raw_smp_processor_id());
		ret = -EAGAIN;
		goto out;
	}

	if (copy_to_user((void __user *)(uintptr_t)req->samples, samples,
			 req->n * sizeof(u64)))
		ret = -EFAULT;

out:
	if (mmio)
		iounmap((void __iomem *)mmio);
	vfree((void *)pollute);
	vfree(samples);
	return ret;
}

static long eb_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct exitbench_req req;
	long ret;

	if (cmd != EXITBENCH_RUN)
		return -ENOTTY;
	if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
		return -EFAULT;

	ret = eb_run(&req);
	if (ret)
		return ret;

	if (copy_to_user((void __user *)arg, &req, sizeof(req)))
		return -EFAULT;
	return 0;
}

static const struct file_operations eb_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = eb_ioctl,
};

static struct miscdevice eb_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "exit-bench",
	.fops  = &eb_fops,
	.mode  = 0666,
};

static int __init eb_init(void)
{
	int ret = misc_register(&eb_dev);

	if (ret)
		return ret;
	pr_info("exit_bench: loaded, /dev/exit-bench ready (fallback mmio gpa 0x%llx)\n",
		fallback_mmio_gpa());
	return 0;
}

static void __exit eb_exit(void)
{
	misc_deregister(&eb_dev);
	pr_info("exit_bench: unloaded\n");
}

module_init(eb_init);
module_exit(eb_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VM exit ladder: guest-kernel rungs for w3_exit_anatomy");
