/*
 * libErrand.h -- the guest side of a Composite errand: one inline vmcall.
 *
 * ABI (CLAUDE.md, verified on cr3_policy d4a7fd99):
 *   rax = (64 + slot + 1) << 16     the capability slot in the VM's captbl
 *   rbx, rsi, rdi, rdx              the four word arguments, in that order
 *   r9 = return address, r8 = fault return address (an invalid capability returns there
 *                                   with rax = -EINVAL; reported as fault = 1)
 *   rax                             the one return value
 *
 * What comes back: the kernel's sret path pops the *server stub's* registers into the guest
 * (vmx_ipc_resume restores the pt_regs of the server's sret syscall, with rax replaced by the
 * return value), exactly as a native Composite invocation leaves the client's registers to
 * the client stub. So after the vmcall every general register except rsp (VMCS) and rax
 * holds a server value; rbp is zeroed by the server entry stub. This wrapper saves and
 * restores rbp itself (stepping over the red zone first), and declares every other register
 * as written: the argument registers are read-write operands, rcx and r8-r15 are clobbered.
 * (ipc_perf.c's wrapper declares fewer clobbers and works by luck of register allocation.)
 *
 * The service may run AES-NI; it zeroes XMM0-15 and restores MXCSR before returning, and the
 * guest's x87 and AVX-upper state is untouched. XMM0-15 are listed as clobbered so the
 * compiler never keeps a live value in them across the call. A kernel build
 * (-mgeneral-regs-only) rejects XMM clobbers; there the caller brackets the call with
 * kernel_fpu_begin/end instead.
 *
 * Works from ring 3 and ring 0 alike: vmcall exits from any CPL.
 */
#ifndef LIBERRAND_H
#define LIBERRAND_H

#define ERRAND_CAP_BASE 64
#define ERRAND_CAP(slot) (((unsigned long)(ERRAND_CAP_BASE + (slot) + 1)) << 16)

#ifdef __KERNEL__
#define ERRAND_XMM_CLOBBERS
#else
#define ERRAND_XMM_CLOBBERS , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", \
	"xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
#endif

static inline unsigned long
errand_call_f(unsigned long cap, unsigned long a0, unsigned long a1, unsigned long a2,
	      unsigned long a3, long *fault)
{
	long f;

	__asm__ __volatile__(
		"leaq -128(%%rsp), %%rsp\n\t" /* step over the red zone before pushing */
		"pushq %%rbp\n\t"
		"movabs $1f, %%r8\n\t"
		"movabs $2f, %%r9\n\t"
		"vmcall\n\t"
		".align 8\n\t"
		"1:\n\t"
		"movl $1, %%ecx\n\t"
		"jmp 3f\n\t"
		"2:\n\t"
		"movl $0, %%ecx\n\t"
		"3:\n\t"
		"popq %%rbp\n\t"
		"leaq 128(%%rsp), %%rsp\n\t"
		: "+a"(cap), "+b"(a0), "+S"(a1), "+D"(a2), "+d"(a3), "=c"(f)
		:
		: "memory", "cc", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" ERRAND_XMM_CLOBBERS);
	if (fault) *fault = f;

	return cap;
}

static inline unsigned long
errand_call(unsigned long slot, unsigned long a0, unsigned long a1, unsigned long a2, unsigned long a3)
{
	return errand_call_f(ERRAND_CAP(slot), a0, a1, a2, a3, 0);
}

#endif /* LIBERRAND_H */
