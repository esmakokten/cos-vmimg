/*
 * r11_probe -- does the guest's r11 reach the errand service as its RFLAGS?
 *
 * The kernel enters the service with sysretq, which loads RFLAGS from r11. This probe is
 * libErrand.h's wrapper with the r11 load made a parameter. For each value it issues
 * TLSKEY_OP_ARGS(a, b, c) on slot 1 (checks the arguments arrive intact) and
 * TLSKEY_OP_COUNTERS (checks the service keeps counting), and prints one line:
 *
 *   r11=<v> args=<packed> expect=<packed> ctr=<n> srv_r11=<x> ok|BAD
 *
 * srv_r11 is the r11 the guest gets back: the fast path returns the server stub's registers
 * (libErrand.h), and the server's r11 at its sret syscall is the server's own RFLAGS at that
 * point. It is a diagnostic only: it shows which RFLAGS the service ran with, it is not part
 * of the ok/BAD verdict.
 *
 * The values are bare bits, so every case but the first also has IF = 0 and IOPL = 0. TF is
 * last: if r11 reaches the service, TF raises #DB at the service's first instruction, the
 * kernel's die() halts the machine, and that case prints its "case" line and nothing after.
 */
#include <stdio.h>
#include "libErrand.h"
#include "tlskey_abi.h"

static inline unsigned long
errand_call_r11(unsigned long slot, unsigned long a0, unsigned long a1, unsigned long a2, unsigned long a3,
		unsigned long *r11_inout)
{
	unsigned long cap = ERRAND_CAP(slot);
	register unsigned long r11 __asm__("r11") = *r11_inout;
	long f;

	__asm__ __volatile__(
		"leaq -128(%%rsp), %%rsp\n\t" /* step over the red zone before pushing */
		"pushq %%rbp\n\t"
		"movabs $1f, %%r8\n\t"
		"movabs $2f, %%r9\n\t"
		"vmcall\n\t" /* r11 = the caller's chosen value, an operand */
		".align 8\n\t"
		"1:\n\t"
		"movl $1, %%ecx\n\t"
		"jmp 3f\n\t"
		"2:\n\t"
		"movl $0, %%ecx\n\t"
		"3:\n\t"
		"popq %%rbp\n\t"
		"leaq 128(%%rsp), %%rsp\n\t"
		: "+a"(cap), "+b"(a0), "+S"(a1), "+D"(a2), "+d"(a3), "=c"(f), "+r"(r11)
		:
		: "memory", "cc", "r8", "r9", "r10", "r12", "r13", "r14", "r15" ERRAND_XMM_CLOBBERS);
	*r11_inout = r11;
	(void)f;

	return cap;
}

static const struct {
	unsigned long r11;
	const char   *name;
} cases[] = {
	{0x3202, "IF IOPL3 (the native value)"},
	{0x400, "DF"},
	{0x0, "IF=0"},
	{0x40000, "AC"},
	{0x100, "TF (last: halts the machine if r11 reaches the service)"},
};

int
main(void)
{
	const unsigned long a = 0x12345, b = 0x6789a, c = 0xbcdef;
	const unsigned long expect = a | (b << 20) | (c << 40);
	unsigned int i, nbad = 0;

	setvbuf(stdout, NULL, _IONBF, 0); /* every line out before a halt */
	printf("r11_probe: %u cases, slot %d, TLSKEY_OP_ARGS + TLSKEY_OP_COUNTERS\n",
	       (unsigned)(sizeof(cases) / sizeof(cases[0])), TLSKEY_SLOT);
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		unsigned long r11 = cases[i].r11, srv, ctr, args, r11c = cases[i].r11;
		int ok;

		printf("r11_probe: case r11=%#lx %s\n", cases[i].r11, cases[i].name);
		args = errand_call_r11(TLSKEY_SLOT, TLSKEY_OP_ARGS, a, b, c, &r11);
		srv  = r11;
		ctr  = errand_call_r11(TLSKEY_SLOT, TLSKEY_OP_COUNTERS, 0, 0, 0, &r11c);
		ok   = args == expect && (long)ctr > 0;
		nbad += !ok;
		printf("r11=%#lx args=%#lx expect=%#lx ctr=%lu srv_r11=%#lx %s\n", cases[i].r11, args, expect, ctr, srv,
		       ok ? "ok" : "BAD");
	}
	printf("r11_probe: DONE, %u BAD\n", nbad);

	return nbad != 0;
}
