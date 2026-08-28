/**
 * ipc_test.c — VM IPC test using vmcall to invoke pong component functions
 *
 * VM IPC Call Convention:
 *   Input:  rax = function slot, rbx = arg0, rcx = arg1, rdx = arg2, rsi = arg3
 *   Output: rax = ret0, rsi = ret1, rdi = ret2
 *
 * Pong Function Slots (configured in VMM):
 *   Slot 0: pong_args(p1,p2,p3,p4) -> returns p1+p2+p3+p4 (sum)
 *   Slot 1: pong_argsrets(p0,p1,p2,p3,*r0,*r1) -> ret0=p2+p3, ret1=p0, ret2=p1
 */

#include <stdint.h>
#include <stdio.h>
#include "stats.h"

static int test_passed = 0;
static int test_failed = 0;

/*
 * vm_ipc_call: issue a vmcall to slot `fn` with 4 arguments.
 * Returns ret0; *r1 and *r2 receive the other two return values.

static inline unsigned long
vm_ipc_call(unsigned long fn,
	    unsigned long a0, unsigned long a1,
	    unsigned long a2, unsigned long a3,
	    unsigned long *r1, unsigned long *r2)
{
	unsigned long ret0, out_r1, out_r2;

	__asm__ volatile (
		"vmcall"
		: "=a"(ret0), "=S"(out_r1), "=D"(out_r2)
		: "a"(fn), "b"(a0), "c"(a1), "d"(a2), "S"(a3)
		: "memory"
	);

	if (r1) *r1 = out_r1;
	if (r2) *r2 = out_r2;
	return ret0;
}
 */

/*
 * vm_ipc_call: issue a vmcall to slot `fn` with 4 arguments.
 * Returns ret0; *r1 and *r2 receive the other two return values.
 */
/*
 * vm_ipc_call: issue a vmcall to slot `fn` with 4 arguments.
 * Returns ret0; *r1 and *r2 receive the other two return values.
 */
#define COS_CAPABILITY_OFFSET 16
#define VM_IPC_SINV_CAP_BASE  64
#define VM_IPC_CAP_ENCODE(slot) \
	(((unsigned long)(VM_IPC_SINV_CAP_BASE + (slot) + 1)) << COS_CAPABILITY_OFFSET)

static inline unsigned long
vm_ipc_call(unsigned long cap_no,
	    unsigned long arg1, unsigned long arg2,
	    unsigned long arg3, unsigned long arg4)
{
	unsigned long ret;
	long fault = 0;

	// Ask Gabe about the align usage
	__asm__ __volatile__(
						"pushq %%rbp\n\t"		\
						 "movabs $1f, %%r8\n\t"	\
						 "movabs $2f, %%r9\n\t"	\
	                     "vmcall\n\t"		\
	                     ".align 8\n\t"		\
	                     "1:\n\t"			\
	                     "movl $0, %%ecx\n\t"	\
	                     "jmp 3f\n\t"		\
	                     "2:\n\t"			\
	                     "movl $1, %%ecx\n\t"	\
	                     "3:\n\t"			\
    					 "popq %%rbp\n\t"
	                     : "=a"(ret), "=c"(fault)
	                     : "a"(cap_no), "b"(arg1), "S"(arg2), "D"(arg3), "d"(arg4), "c"(0)
	                     : "memory", "cc", "r8", "r9", "r11","r12");
	return ret;
}


static void
test_check(const char *name, unsigned long expected, unsigned long actual)
{
	if (expected == actual) {
		printf("  ✓ %s: PASS (expected=%lu, actual=%lu)\n", name, expected, actual);
		test_passed++;
	} else {
		printf("  ✗ %s: FAIL (expected=%lu, actual=%lu)\n", name, expected, actual);
		test_failed++;
	}
}

void main(void)
{
	unsigned long ret0, ret1, ret2;

	printf("\n");
	printf("╔═══════════════════════════════════════════════════════════╗\n");
	printf("║         VM IPC Test - vmcall → Pong Component            ║\n");
	printf("╚═══════════════════════════════════════════════════════════╝\n");
	printf("\n");

	/* Test 1: pong_args(p1,p2,p3,p4) - sum of 4 arguments */
	printf("Test 1: pong_args(p1,p2,p3,p4) - should return sum\n");
	ret0 = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
	printf("  vmcall(fn=0, 10, 20, 30, 40) → ret0=%lu\n", ret0);
	test_check("pong_args(10,20,30,40)", 100, ret0);

	ret0 = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 1, 2, 3, 4);
	printf("  vmcall(fn=0, 1, 2, 3, 4) → ret0=%lu\n", ret0);
	test_check("pong_args(1,2,3,4)", 10, ret0);

	ret0 = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 42, 100, 0, 0);
	printf("  vmcall(fn=0, 42, 100, 0, 0) → ret0=%lu\n", ret0);
	test_check("pong_args(42,100,0,0)", 142, ret0);

	ret0 = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 0, 0, 0, 0);
	printf("  vmcall(fn=0, 0, 0, 0, 0) → ret0=%lu\n", ret0);
	test_check("pong_args(0,0,0,0)", 0, ret0);
	printf("\n");

	/* Test 2: pong_argsrets(p0,p1,p2,p3) - multiple return values
	 * Returns: ret0 = p2+p3, ret1 = p0, ret2 = p1 
	printf("Test 2: pong_argsrets(p0,p1,p2,p3) - multiple returns\n");
	printf("  Expected: ret0=p2+p3, ret1=p0, ret2=p1\n");
	ret0 = vm_ipc_call(1, 42, 100, 5, 7, &ret1, &ret2);
	printf("  vmcall(fn=1, 42, 100, 5, 7) → ret0=%lu, ret1=%lu, ret2=%lu\n", 
	       ret0, ret1, ret2);
	test_check("pong_argsrets ret0 (5+7)", 12, ret0);
	test_check("pong_argsrets ret1 (42)", 42, ret1);
	test_check("pong_argsrets ret2 (100)", 100, ret2);

	ret0 = vm_ipc_call(1, 1, 2, 3, 4, &ret1, &ret2);
	printf("  vmcall(fn=1, 1, 2, 3, 4) → ret0=%lu, ret1=%lu, ret2=%lu\n", 
	       ret0, ret1, ret2);
	test_check("pong_argsrets ret0 (3+4)", 7, ret0);
	test_check("pong_argsrets ret1 (1)", 1, ret1);
	test_check("pong_argsrets ret2 (2)", 2, ret2);
	printf("\n");*/

	/* Test 3: Edge cases and large values
	printf("Test 3: Edge cases\n");
	ret0 = vm_ipc_call(0, 1000, 2000, 3000, 4000, &ret1, &ret2);
	printf("  vmcall(fn=0, 1000, 2000, 3000, 4000) → ret0=%lu\n", ret0);
	test_check("pong_args(1000,2000,3000,4000)", 10000, ret0);

	ret0 = vm_ipc_call(1, 99, 88, 77, 66, &ret1, &ret2);
	printf("  vmcall(fn=1, 99, 88, 77, 66) → ret0=%lu, ret1=%lu, ret2=%lu\n",
	       ret0, ret1, ret2);
	test_check("pong_argsrets ret0 (77+66)", 143, ret0);
	test_check("pong_argsrets ret1 (99)", 99, ret1);
	test_check("pong_argsrets ret2 (88)", 88, ret2);
	printf("\n"); */

	/* Summary */
	printf("╔═══════════════════════════════════════════════════════════╗\n");
	printf("║                      Test Summary                         ║\n");
	printf("╠═══════════════════════════════════════════════════════════╣\n");
	printf("║  Passed: %-4d                                             ║\n", test_passed);
	printf("║  Failed: %-4d                                             ║\n", test_failed);
	printf("║  Total:  %-4d                                             ║\n", test_passed + test_failed);
	printf("╠═══════════════════════════════════════════════════════════╣\n");
	if (test_failed == 0) {
		printf("║  Result: ✓ ALL TESTS PASSED                              ║\n");
	} else {
		printf("║  Result: ✗ SOME TESTS FAILED                             ║\n");
	}
	printf("╚═══════════════════════════════════════════════════════════╝\n");
	printf("\n");
	printf("[vm_ipc_test] DONE\n\n");

}
