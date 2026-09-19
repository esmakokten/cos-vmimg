/*
 * exitbench_rungs.h -- the timing harness and the exit rungs themselves.
 * Included from the guest kernel module and from the userspace driver, so it
 * must compile in both. Nothing here allocates or prints.
 *
 * TIMING. The predecessor harness (helpers@345b399, programs/stats.h users)
 * opened every sample with `cpuid; rdtsc`. CPUID *is itself a VM exit*, so
 * every measured exit was preceded ~50 cycles earlier by a warming exit through
 * the very code being measured: I-cache, branch predictors and KVM's own data
 * structures were hot in a way no real workload reproduces. That biases every
 * KVM number downward.
 *
 * We use `lfence; rdtsc; lfence` to open and `rdtscp; lfence` to close. LFENCE
 * is architecturally dispatch-serialising on Intel parts carrying the Spectre
 * v1 microcode (all of Skylake-SP as shipped), and unlike CPUID it does not
 * exit. RDTSCP already waits for older instructions to retire; the trailing
 * LFENCE stops younger ones from reading the clock early.
 *
 * RDTSC is not intercepted by KVM by default (TSC offsetting is used instead),
 * so guest cycles are host cycles plus a constant -- differences are exact.
 * Confirm with the L0 rung: if RDTSC were intercepted, L0 would cost thousands
 * of cycles rather than ~30-90.
 */
#ifndef EXITBENCH_RUNGS_H
#define EXITBENCH_RUNGS_H

#include "exitbench_abi.h"

#ifdef __KERNEL__
#include <linux/types.h>
typedef u64 eb_u64;
#else
#include <stdint.h>
typedef uint64_t eb_u64;
#endif

#define MSR_IA32_TSC_DEADLINE_EB  0x6e0
#define MSR_IA32_UCODE_REV_EB     0x8b

/* An unknown hypercall number. KVM's kvm_emulate_hypercall() rejects it with
 * -KVM_ENOSYS after the full exit-handler dispatch, which is exactly the path
 * we want to price. The predecessor harness executed a bare `vmcall` without
 * setting RAX, so whatever the compiler had left there selected the hypercall
 * -- occasionally a real one (KVM_HC_KICK_CPU and friends do work). Pin it. */
#define EB_VMCALL_NR  0x7fffffffUL

static inline eb_u64 eb_tsc_begin(void)
{
	unsigned int a, d;
	__asm__ __volatile__("lfence" ::: "memory");
	__asm__ __volatile__("rdtsc" : "=a"(a), "=d"(d));
	__asm__ __volatile__("lfence" ::: "memory");
	return ((eb_u64)d << 32) | a;
}

static inline eb_u64 eb_tsc_end(void)
{
	unsigned int a, d, c;
	__asm__ __volatile__("rdtscp" : "=a"(a), "=d"(d), "=c"(c) :: "memory");
	__asm__ __volatile__("lfence" ::: "memory");
	return ((eb_u64)d << 32) | a;
}

/*
 * Cold mode. A tight exit loop measures the steady state of a microarchitecture
 * that has seen nothing but this exit for the last million iterations. Real
 * guests exit from the middle of their own working set. `eb_pollute` walks a
 * buffer and runs a hard-to-predict branch chain between samples so the rung is
 * priced with the guest's state evicted -- the honest upper bound to sit beside
 * the tight-loop lower bound. The delta between the two is the systematic
 * optimism of every tight-loop exit benchmark in the literature, this project's
 * own earlier numbers included.
 */
#define EB_POLLUTE_BYTES (2u << 20)

static inline void eb_pollute(volatile unsigned char *buf, eb_u64 seed)
{
	unsigned int i;
	unsigned int x = (unsigned int)seed | 1u;

	if (!buf)
		return;
	for (i = 0; i < EB_POLLUTE_BYTES; i += 64) {
		buf[i] += (unsigned char)x;
		x = x * 1103515245u + 12345u;
		if (x & 0x10000u)          /* unpredictable by construction */
			buf[i] ^= 0xa5;
	}
}

/*
 * The rungs. Each returns nothing and must be free of side effects that
 * accumulate across a million iterations.
 */

static inline void eb_rung_l0_rdtsc(void)
{
	/* nothing: the interval brackets only the harness itself */
}

/* handle_fastpath_set_msr_irqoff() services MSR_IA32_TSC_DEADLINE without
 * unwinding to vcpu_enter_guest() and re-enters with
 * EXIT_FASTPATH_REENTER_GUEST.
 *
 * MEASURED, AND NOT WHAT THE NAME SUGGESTS. On .154 this rung costs ~4,060
 * cycles against ~3,864 for a no-op VMCALL -- the "fast path" is SLOWER. Two
 * readings remain open and the path gate (run.sh --trace, needs root)
 * distinguishes them:
 *
 *   (a) the fast path is not being taken at all, or
 *   (b) it is taken, and it is slower anyway, because servicing the write means
 *       reprogramming the LAPIC timer -- cancel_apic_timer(), start_apic_timer(),
 *       a vmcs_write to arm the VMX preemption timer -- work a null VMCALL
 *       never does.
 *
 * If (b), then this rung is not "KVM's software floor" and must not be quoted
 * as one: it is the cheapest exit KVM actually *services*, which is a different
 * and still useful quantity. L2 remains the floor for null work.
 *
 * (A first fix attempt assumed the deadline was out of the preemption timer's
 * ~1.4e11-tick range, forcing the start_sw_tscdeadline() hrtimer fallback.
 * Shortening it from 2^40 to 2^31 changed the p50 by nothing, so that was not
 * the cause. 2^31 is kept: it is correct regardless, and ~1s at 2.1 GHz is
 * still far beyond any single sample.)
 *
 * The caller saves and restores the guest's own deadline around the loop. */
static inline void eb_rung_l1_fastpath_msr(eb_u64 deadline)
{
	unsigned int lo = (unsigned int)deadline;
	unsigned int hi = (unsigned int)(deadline >> 32);

	__asm__ __volatile__("wrmsr"
			     :: "c"(MSR_IA32_TSC_DEADLINE_EB), "a"(lo), "d"(hi)
			     : "memory");
}

static inline void eb_rung_l2_vmcall(void)
{
	unsigned long nr = EB_VMCALL_NR;

	__asm__ __volatile__("vmcall"
			     : "+a"(nr)
			     : "b"(0UL), "c"(0UL), "d"(0UL)
			     : "memory");
}

static inline void eb_rung_l3_cpuid(void)
{
	unsigned int a = 0, b, c, d;

	__asm__ __volatile__("cpuid"
			     : "+a"(a), "=b"(b), "=c"(c), "=d"(d)
			     :: "memory");
}

/* MSR_IA32_UCODE_REV is never passed through the MSR bitmap: KVM emulates it
 * from vcpu->arch.microcode_version, so the read always exits and always lands
 * in the full handler. */
static inline void eb_rung_l4_rdmsr(void)
{
	unsigned int lo, hi;

	__asm__ __volatile__("rdmsr"
			     : "=a"(lo), "=d"(hi)
			     : "c"(MSR_IA32_UCODE_REV_EB)
			     : "memory");
	(void)lo; (void)hi;
}

/* Only meaningful with an interrupt already pending, so KVM's handle_halt()
 * returns without blocking. Called with interrupts *enabled* and a deadline
 * already expired; otherwise the vCPU blocks and the sample measures a
 * scheduler round trip, not an exit. Off by default for that reason. */
static inline void eb_rung_l5_hlt(void)
{
	__asm__ __volatile__("hlt" ::: "memory");
}

/* Port 0xE9. With `-device isa-debugcon,iobase=0xe9,chardev=null` QEMU accepts
 * the byte and drops it, so the sample prices the round trip and not a write(). */
static inline void eb_rung_l6_pio(void)
{
	__asm__ __volatile__("outb %b0, %w1"
			     :: "a"((unsigned char)'T'), "Nd"((unsigned short)0xe9)
			     : "memory");
}

/* A store to a guest-physical address with no memslot behind it. KVM cannot
 * resolve it, so it exits to userspace with KVM_EXIT_MMIO. Unlike PIO this is
 * independent of the machine model -- it behaves identically under q35,
 * -M microvm and a hand-written VMM -- which is why it, and not L6, is the rung
 * carried across VMM configurations. */
static inline void eb_rung_l7_mmio(volatile unsigned int *mmio)
{
	*mmio = 0x5a5a5a5au;
	__asm__ __volatile__("" ::: "memory");
}

#endif /* EXITBENCH_RUNGS_H */
