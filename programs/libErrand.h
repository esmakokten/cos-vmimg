/*
 * libErrand.h -- the guest side of a Composite errand: one inline vmcall.
 *
 * ABI (CLAUDE.md, verified on cr3_policy d4a7fd99):
 *   rax = (64 + 4*slot + 1) << 16   the capability slot in the VM's captbl (sinv caps are 4 ids wide)
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
 * r11 on the way in. The fast path enters the server with sysretq, which loads RFLAGS from
 * r11. Kernels before the w8 fix (composite 947619e1, "one RFLAGS value for every sysret")
 * did not set r11 on that path, so the server started with whatever the guest left there:
 * from the guest kernel it was a pointer, whose bit 8 (TF) made the server's first
 * instruction trap (#DB) and halted the host. A fixed kernel loads 0x3200 itself and ignores
 * this value (w8 step 1.4 ran without the load: unchanged). The load stays, belt and braces
 * (w5 DECISIONS A6), so this wrapper is also safe on an unfixed kernel; 0x3202 is the value
 * a fixed kernel gives the server anyway (IF, IOPL 3; bit 1 always reads as set).
 *
 * Works from ring 3 and ring 0 alike: vmcall exits from any CPL.
 */
#ifndef LIBERRAND_H
#define LIBERRAND_H

#define ERRAND_CAP_BASE   64
#define ERRAND_CAP_STRIDE 4 /* a sinv capability is 64 B = 4 capability ids */
#define ERRAND_CAP(slot) (((unsigned long)(ERRAND_CAP_BASE + ERRAND_CAP_STRIDE * (slot) + 1)) << 16)

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
		"movq $0x3202, %%r11\n\t" /* belt and braces for unfixed kernels: see above */
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

#ifndef __KERNEL__
/*
 * w11 Phase 6 (DECISIONS A71, A76, A79, A80): the process registers itself with the VMM; no driver.
 *
 * errand_self_register(comp, touch, &err):
 *   1. mlockall(MCL_CURRENT | MCL_FUTURE): the process's obligation (A79). Without it Linux's next
 *      write to the page tables revokes the registration (it fails closed).
 *   2. One anonymous private page, read+exec, populated: the address of the gate. Linux knows the
 *      address is taken; the VMM writes the mapping (A76).
 *   3. One byte read from it, unless touch is 0 (T15's second arm).
 *   4. The cpuid leaf from ring 3. The root is this exit's CR3 (A72).
 *
 * Returns the gate's address (call it as ipc_perf's vm_gate_call does: rax = the slot's capability,
 * rcx = 0, arguments in rbx, rsi, rdi, rdx; `call` with rsp below the red zone), or NULL with *err =
 * the VMM's negative errno, or -1 when mlockall or mmap failed. errand_self_unregister() gives it
 * back: the VMM restores the process's own entry for the page.
 */
#include <sys/mman.h>
#include "errand_id_abi.h"

static inline long
errand_self_cpuid(unsigned int op, unsigned long gate_va, unsigned long comp)
{
	unsigned int a = ERRAND_ID_CPUID_LEAF, b, c = op, d;

	__asm__ __volatile__("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d) : "S"(gate_va), "D"(comp) : "memory");
	return (long)(int)a;
}

/* Steps 1 to 3: the address, reserved and populated, or NULL */
static inline void *
errand_self_reserve(int touch)
{
	volatile unsigned char *g;

	if (mlockall(MCL_CURRENT | MCL_FUTURE)) return 0;
	g = (volatile unsigned char *)mmap(0, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if ((void *)g == MAP_FAILED) return 0;
	if (touch) (void)g[0];
	return (void *)g;
}

static inline void *
errand_self_register(unsigned long comp, int touch, long *err)
{
	void *g = errand_self_reserve(touch);

	if (!g) {
		*err = -1;
		return 0;
	}
	*err = errand_self_cpuid(ERRAND_ID_SELF_REGISTER, (unsigned long)g, comp);
	return *err ? 0 : g;
}

static inline long
errand_self_unregister(void)
{
	return errand_self_cpuid(ERRAND_ID_SELF_UNREGISTER, 0, 0);
}
#endif /* !__KERNEL__ */

#endif /* LIBERRAND_H */
