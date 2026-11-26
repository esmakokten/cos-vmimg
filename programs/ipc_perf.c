/**
 * ipc_perf.c — VM IPC Performance Benchmark using vmcall
 *
 * VM IPC Call Convention:
 *   Input:  rax = function slot, rbx = arg0, rcx = arg1, rdx = arg2, rsi = arg3
 *   Output: rax = ret0, rsi = ret1, rdi = ret2
 *
 * Pong Function Slots (configured in VMM):
 *   Slot 0: pong_args(p1,p2,p3,p4) -> returns p1+p2+p3+p4 (sum)
 *   Slot 1: pong_argsrets(p0,p1,p2,p3,*r0,*r1) -> ret0=p2+p3, ret1=p0, ret2=p1
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <malloc.h>
#include <time.h>
#include <errno.h>
#include "stats.h"
#include "errand_id_abi.h"
#include "libErrand.h" /* Phase 6: errand_self_register, errand_self_cpuid */

/* Serialized RDTSC measurements */
static inline uint64_t rdtsc_serialized_start(void) {
	unsigned int a, d;
	asm volatile("cpuid" : : "a"(0) : "rbx","rcx","rdx");
	asm volatile("rdtsc" : "=a"(a), "=d"(d));
	return ((uint64_t)d<<32) | a;
}

static inline uint64_t rdtsc_serialized_end(void) {
	unsigned int a, d, c;
	asm volatile("rdtscp" : "=a"(a), "=d"(d), "=c"(c));
	asm volatile("lfence");
	return ((uint64_t)d<<32) | a;
}

/* CPU pinning and memory locking for more stable measurements */
static void pin_cpu0(void) {
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(0, &set);
	sched_setaffinity(0, sizeof(set), &set);
}

static void lock_mem(void) {
	mlockall(MCL_CURRENT | MCL_FUTURE);
}

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
#define COS_CAPABILITY_OFFSET 16
#define VM_IPC_SINV_CAP_BASE  64
/* A sinv capability is 64 B = 4 ids: slot s is at id 64 + 4s (see libErrand.h). */
#define VM_IPC_CAP_ENCODE(slot) \
	(((unsigned long)(VM_IPC_SINV_CAP_BASE + 4 * (slot) + 1)) << COS_CAPABILITY_OFFSET)

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
						 "movq $0x3202, %%r11\n\t" /* only needed on unfixed kernels: see libErrand.h */ \
	                     "vmcall\n\t"		\
	                     ".align 8\n\t"		\
	                     "1:\n\t"			\
	                     "movl $0, %%ecx\n\t"	\
	                     "jmp 3f\n\t"		\
	                     "2:\n\t"			\
	                     "movl $1, %%ecx\n\t"	\
	                     "3:\n\t"			\
    					 "popq %%rbp\n\t"
	                     /*
	                      * rbx, rsi, rdi, rdx are read-write: the errand returns values in rsi
	                      * and rdi (and the return path may change the others), so they are not
	                      * the arguments any more after the vmcall. As inputs only, a loop kept
	                      * its constant arguments in them and passed the previous call's returns
	                      * (w11, BUGS.md).
	                      */
	                     : "=a"(ret), "=c"(fault), "+b"(arg1), "+S"(arg2), "+D"(arg3), "+d"(arg4)
	                     : "a"(cap_no), "c"(0)
	                     : "memory", "cc", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"); /* as libErrand.h */
	return ret;
}

/*
 * w11 Phase 3: the call through the gate (DECISIONS A32 to A34). The gate is one page of
 * /dev/errand_id mapped read+exec; the VMM writes the stub into it at registration. The registers
 * are the inline wrapper's; the stub saves rbp, loads r11, executes the gate's one vmcall and
 * returns. The 128-byte red zone below rsp is skipped, because call pushes into it.
 */
static unsigned long gate_fn; /* the gate's address once registered through it; 0 = inline vmcall */

/*
 * T11/T12: did a call carry the module's marker? Through rax (a forged result) it comes back whole;
 * through rbx (a forged argument) pong_args returns the sum as an int, so its low 32 bits.
 */
#define T11_MARKED(v) ((v) == ERRAND_ID_T11_MARK || ((v) & 0xffffffffUL) == (ERRAND_ID_T11_MARK & 0xffffffffUL))

static inline unsigned long
vm_gate_call(unsigned long cap_no, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4)
{
	unsigned long ret;
	long          fault;

	__asm__ __volatile__("subq $128, %%rsp\n\t"
	                     "callq *%[g]\n\t"
	                     "addq $128, %%rsp\n\t"
	                     : "=a"(ret), "=c"(fault), "+b"(arg1), "+S"(arg2), "+D"(arg3), "+d"(arg4)
	                     : "a"(cap_no), "c"(0), [g] "m"(gate_fn)
	                     : "memory", "cc", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15");
	(void)fault;
	return ret;
}

/*
 * The same 12 bytes as the gate's stub, in ipc_perf's own text (3.10's split: the stub's cost
 * without the kernel's gate check, on a kernel without VMCALL_GATE).
 */
void local_stub(void);
__asm__(".text\n.globl local_stub\nlocal_stub:\n\t"
        ".byte 0x55, 0x41, 0xbb, 0x02, 0x32, 0x00, 0x00, 0x0f, 0x01, 0xc1, 0x5d, 0xc3\n");

/* Through the gate when the process registered with one, else the inline vmcall. */
static inline unsigned long
vm_call(unsigned long cap_no, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4)
{
	return gate_fn ? vm_gate_call(cap_no, arg1, arg2, arg3, arg4) : vm_ipc_call(cap_no, arg1, arg2, arg3, arg4);
}

/*
 * w11 (identity): who may make the vmcall.
 *   --register        open /dev/errand_id and register this process before running
 *   --no-register     run unregistered (the process Q); prints its root
 *   --want-bit12      with --no-register: exit 3 at once unless the root has bit 12 set (A8)
 *   --attempts N      instead of the benchmark: N calls to slot 0, counted served/refused/other
 *   --hold            after the run, keep the registration until SIGTERM, then unregister
 *   --kernel-vmcall N N ring-0 vmcalls with slot 0 in rax, from the module (T2)
 *   --counters        ask the VMM to print its identity counters
 *   --soak-fork N     Q10: N rounds of fork; the child registers, prints one line, makes 100
 *                     checked calls, unregisters and exits (a new process and PGD each round)
 *   --soak-loop N     Q10: one process, N rounds of register, 100 checked calls, unregister
 * Phase 2 (page-table lock):
 *   --register-locked register with the page tables locked (THP off, mlockall, every allocation
 *                     made first); a later allocation or page fault would revoke the registration
 *   --no-preset       with --register-locked: the module does not pre-set accessed/dirty (T3 arm B)
 *   --heap-mb N       allocate and touch N MiB of heap before registering (T9)
 *   --seconds N       instead of the benchmark: call slot 0 for N seconds, counted (T3); prints the
 *                     background load's progress (/tmp/spawn.cnt, /tmp/churn.cnt) at the window's start
 *                     and end; works with --register and --register-locked
 *   --no-cpuid        with --seconds: no cpuid in the loop (T3 run-1 form: fast-path exits only)
 *   --t4              after registering: have the module rewrite one of our code-page entries, then
 *                     10^4 calls counted, then the entry restored (T4)
 *   --exit-registered exit without unregistering (T7)
 *   --churn           forever: map 16 MiB, touch it, unmap it (T3's background load)
 *   --dump            ask the VMM to print its page-table-lock counters
 * Phase 3 (the gate):
 *   --gate C          register locked with a gate page, component C (0: slots 0+1, 1: slot 0, 2: slot 1);
 *                     every call then goes through the gate
 *   --gate-drop       with --gate: the module leaves one lower table out of the list (3.2: refused)
 *   --attempts-inline N  N slot-0 calls with the old inline vmcall, outside the gate (T2-gate)
 *   --t5              the faked calls: the module returns this thread to a vmcall outside the gate,
 *                     to the gate's first byte, and to the gate's vmcall, with its own registers
 *   --t6              the module writes the gate page; then 10^4 calls, and the gate's bytes read
 *   --t8              after registering, wait for /tmp/t8.peer, then 10^4 calls to each slot
 * Phase 4 (an interrupt inside the gate):
 *   --t11 N           after registering with a gate: N seconds of gate calls with a changing first
 *                     argument, every result checked; the module's 1 kHz timer counts what it sees
 *   --t11-tamper      with --t11: the module also writes a marker into the frame (T11)
 *   --cpuid-cost      the median of 100,000 timed user-mode cpuid round trips (4.8)
 *   --wake N          for N seconds, sleep to absolute 1 ms deadlines and record how late each
 *                     wake-up is (A55's wake-latency loop); prints p50, p99 and max in microseconds
 *   --t12             after registering with a gate: the module single-steps this thread into the
 *                     gate's vmcall and writes the marker there (T12)
 *   --stub-jmp        with --register-locked: the timed slot-0 call through a copy of the stub
 *                     entered by a direct jmp and left by a direct jmp (4.10), 100,000 samples
 *   --stub-local      with --register-locked: call through a copy of the gate's stub in ipc_perf's
 *                     own text (no VMCALL_GATE needed): the stub's cost alone (3.10)
 * With no flag, ipc_perf is the T0 benchmark it always was.
 */
static int id_fd = -1;
static uint64_t *bench_pre0, *bench_pre1; /* the benchmark's buffers, allocated before a locked registration */
static int id_exit_registered;
static int id_no_cpuid;

/* A counter the background load keeps in a file; read with no allocation (a locked P must not). */
static long
load_count(const char *path)
{
	static char buf[32];
	long        n;
	int         fd = open(path, O_RDONLY);

	if (fd < 0) return -1;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0) return -1;
	buf[n] = 0;
	return strtol(buf, 0, 10);
}
static volatile sig_atomic_t id_stop;

static void id_on_term(int sig) { (void)sig; id_stop = 1; }

static int
id_open(void)
{
	if (id_fd < 0) id_fd = open("/dev/errand_id", O_RDWR);
	if (id_fd < 0) printf("ipc_perf: cannot open /dev/errand_id\n");
	return id_fd;
}

/* Phase 6: the module, if the image has it, without complaint when it does not */
static int
id_open_quiet(void)
{
	if (id_fd < 0) id_fd = open("/dev/errand_id", O_RDWR);
	return id_fd;
}

static unsigned long
id_root(void)
{
	unsigned long root = 0;

	if (id_open_quiet() >= 0 && ioctl(id_fd, ERRAND_ID_IOC_ROOT, &root)) root = 0;
	return root;
}

static int id_self; /* Phase 6: registered itself with the cpuid leaf */

/* 1,000 checked calls per slot before anything is timed: the correctness gate on the real path. */
static int
id_check(void)
{
	int i, bad0 = 0, bad1 = 0;

	for (i = 0; i < 1000; i++) {
		if (vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40) != 100) bad0++;
		if (vm_call(VM_IPC_CAP_ENCODE(1), 42, 100, 5, 7) != 12) bad1++;
	}
	printf("ipc_perf: check 1000 calls per slot: slot 0 bad %d, slot 1 bad %d: %s\n",
	       bad0, bad1, (bad0 || bad1) ? "CHECK FAILED" : "CHECK PASSED");
	return bad0 || bad1;
}

static void
id_attempts(unsigned long n)
{
	unsigned long i, served = 0, refused = 0, other = 0, last = 0;

	for (i = 0; i < n; i++) {
		last = vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
		if (last == 100) served++;
		else if ((long)last == -1 /* -EPERM */) refused++;
		else other++;
	}
	printf("ipc_perf: attempts pid %d root %#lx: %lu calls, served %lu, refused %lu, other %lu (last %ld)\n",
	       getpid(), id_root(), n, served, refused, other, (long)last);
}

/* One soak round: register, 100 checked calls to slot 0, unregister. Returns 0 when all of it held. */
static int
soak_round(int print, unsigned long round)
{
	long r, u;
	int  i, bad = 0;

	r = ioctl(id_fd, ERRAND_ID_IOC_REGISTER);
	if (print) {
		printf("soak %lu pid %d root %#lx register %ld\n", round, getpid(), id_root(), r);
		fflush(stdout);
	}
	if (r) return 1;
	for (i = 0; i < 100; i++) {
		if (vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40) != 100) bad++;
	}
	u = ioctl(id_fd, ERRAND_ID_IOC_UNREGISTER);
	return bad || u;
}

static int
soak_fork(unsigned long n)
{
	unsigned long i, ok = 0, fail = 0;

	for (i = 0; i < n; i++) {
		pid_t pid = fork();
		int   st;

		if (pid == 0) {
			id_fd = -1; /* the child opens its own file */
			if (id_open() < 0) _exit(2);
			_exit(soak_round(1, i));
		}
		if (pid < 0 || waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st)) fail++;
		else ok++;
	}
	printf("ipc_perf: soak-fork done: %lu rounds, ok %lu, fail %lu\n", n, ok, fail);
	fflush(stdout);
	return fail != 0;
}

static int
soak_loop(unsigned long n)
{
	unsigned long i, ok = 0, fail = 0;

	if (id_open() < 0) return 1;
	for (i = 0; i < n; i++) {
		if (soak_round(0, i)) fail++;
		else ok++;
	}
	printf("ipc_perf: soak-loop done: %lu rounds, ok %lu, fail %lu\n", n, ok, fail);
	fflush(stdout);
	return fail != 0;
}

/* Everything a locked process will touch is mapped and faulted before it registers. */
static void
lock_prepare(unsigned long heap_mb, int bench)
{
	volatile char   stack[256 * 1024];
	struct timespec ts;
	unsigned long   i;

	prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);
	for (i = 0; i < sizeof(stack); i += 4096) stack[i] = 1; /* grow the stack now */
	if (heap_mb) {
		char *h = malloc(heap_mb << 20);

		if (h) memset(h, 1, heap_mb << 20);
	}
	if (bench) {
		/*
		 * glibc's qsort takes a merge buffer the size of the array (800 KB) from malloc, which
		 * would mmap it after registration and revoke P (T1-locked run 1: a write in
		 * __handle_mm_fault). Keep large blocks on the heap, never trim it, and grow it now.
		 */
		void *grow;

		mallopt(M_MMAP_THRESHOLD, 64 << 20);
		mallopt(M_TRIM_THRESHOLD, -1);
		bench_pre0 = aligned_alloc(64, 100000 * sizeof(uint64_t));
		bench_pre1 = aligned_alloc(64, 100000 * sizeof(uint64_t));
		if (bench_pre0) memset(bench_pre0, 0, 100000 * sizeof(uint64_t));
		if (bench_pre1) memset(bench_pre1, 0, 100000 * sizeof(uint64_t));
		/* after the buffers, so the freed block stays free for qsort */
		if ((grow = malloc(4 << 20))) {
			memset(grow, 0, 4 << 20);
			free(grow);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &ts); /* the vDSO data page */
	pin_cpu0();
	mlockall(MCL_CURRENT | MCL_FUTURE);
	printf("ipc_perf: pid %d prepared for a locked registration (heap %lu MiB)\n", getpid(), heap_mb);
	fflush(stdout);
}

static void
id_seconds(unsigned long secs)
{
	unsigned long   n = 0, served = 0, refused = 0, other = 0, last = 0;
	struct timespec t0, t;

	long            spawn0 = load_count("/tmp/spawn.cnt"), churn0 = load_count("/tmp/churn.cnt");

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (;;) {
		last = vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
		if (last == 100) served++;
		else if ((long)last == -1) refused++;
		else other++;
		if ((++n & 0xfff) == 0) {
			unsigned int a = 0, b, c = 0, d;

			/*
			 * One slow exit per 4,096 calls: the guest's timer is injected only at a slow-path
			 * entry, so a loop of fast-path calls alone starves the guest's clock (T3 run 1, an
			 * RCU stall and a hang; BUGS.md). The benchmark's own cpuid does the same.
			 */
			if (!id_no_cpuid) __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
			clock_gettime(CLOCK_MONOTONIC, &t);
			if ((unsigned long)(t.tv_sec - t0.tv_sec) >= secs) break;
		}
	}
	printf("ipc_perf: %lu s pid %d%s: %lu calls, served %lu, refused %lu, other %lu (last %ld)\n", secs, getpid(),
	       id_no_cpuid ? " (no cpuid)" : "", n, served, refused, other, (long)last);
	printf("ipc_perf: load during the window: processes spawned %ld -> %ld, churn rounds %ld -> %ld\n", spawn0,
	       load_count("/tmp/spawn.cnt"), churn0, load_count("/tmp/churn.cnt"));
}

static void ipc_perf_bench(void);

static void
id_t4(void)
{
	unsigned long va = (unsigned long)&ipc_perf_bench, i, served = 0, refused = 0, other = 0, r;
	long          ret;

	ret = ioctl(id_fd, ERRAND_ID_IOC_T4_REWRITE, va);
	for (i = 0; i < 10000; i++) {
		r = vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
		if (r == 100) served++;
		else if ((long)r == -1) refused++;
		else other++;
	}
	printf("ipc_perf: T4 pid %d: rewrite of the entry for %#lx: %ld; then 10000 calls: served %lu, refused %lu, other %lu\n",
	       getpid(), va, ret, served, refused, other);
	printf("ipc_perf: T4 restore: %d\n", ioctl(id_fd, ERRAND_ID_IOC_T4_REWRITE, va | 1));
}

/*
 * T5 harness. t5_run(fd, arg) saves the callee-saved registers and rsp, then issues the T5 ioctl
 * by a raw syscall; the module makes the thread return to user mode at arg->rip with its own
 * registers. Every target ends at t5_return, which records rax and restores the saved context.
 */
unsigned long t5_saved_rsp, t5_rax;
unsigned long t5_stack[64] __attribute__((aligned(64)));
long t5_run(long fd, struct errand_id_t5 *arg);
void t5_outside(void);
void t5_return(void);
__asm__(".text\n"
        ".globl t5_outside\n"
        "t5_outside:\n\t"
        "vmcall\n\t"                 /* (a): a vmcall that is not the gate's */
        "nop; nop; nop\n\t"
        "jmp t5_return\n"
        ".globl t5_return\n"
        "t5_return:\n\t"
        "movq %rax, t5_rax(%rip)\n\t"
        "movq t5_saved_rsp(%rip), %rsp\n\t"
        "popq %r15\n\tpopq %r14\n\tpopq %r13\n\tpopq %r12\n\tpopq %rbp\n\tpopq %rbx\n\t"
        "movq t5_rax(%rip), %rax\n\t"
        "ret\n"
        ".globl t5_run\n"
        "t5_run:\n\t"
        "pushq %rbx\n\tpushq %rbp\n\tpushq %r12\n\tpushq %r13\n\tpushq %r14\n\tpushq %r15\n\t"
        "movq %rsp, t5_saved_rsp(%rip)\n\t"
        "movq %rsi, %rdx\n\t"
        "movl $0xE1D0000B, %esi\n\t"
        "movl $16, %eax\n\t"          /* SYS_ioctl; rdi = fd */
        "syscall\n\t"
        "jmp t5_return\n");

static void
id_t5(void)
{
	struct errand_id_t5 t;
	const char         *what[3] = { "(a) a vmcall outside the gate", "(b) the gate's first instruction",
	                                "(c) the gate's vmcall instruction" };
	int                 k;

	for (k = 0; k < 3; k++) {
		long r;

		memset(t5_stack, 0, sizeof(t5_stack));
		t.rax = VM_IPC_CAP_ENCODE(0);
		t.rcx = 0;
		t.rbx = 1000;
		t.rsi = 2000;
		t.rdi = 3000;
		t.rdx = 4000;
		t.rsp = (unsigned long)&t5_stack[32];
		if (k == 0) t.rip = (unsigned long)t5_outside;
		if (k == 1) { t.rip = gate_fn; t5_stack[32] = (unsigned long)t5_return; }
		if (k == 2) { t.rip = gate_fn + ERRAND_ID_GATE_VMCALL_OFF; t5_stack[33] = (unsigned long)t5_return; }
		r = t5_run(id_fd, &t);
		printf("ipc_perf: T5 %s with the module's arguments 1000, 2000, 3000, 4000: rax %ld (%s)\n", what[k], r,
		       r == 10000 ? "SERVED: the service saw the module's arguments" : r == -1 ? "refused" : "other");
		fflush(stdout);
		ioctl(id_fd, ERRAND_ID_IOC_COUNTERS);
	}
}

static void
id_t6(void)
{
	unsigned long i, served = 0, refused = 0, other = 0, r;
	long          b;
	unsigned char *g = (unsigned char *)gate_fn;

	ioctl(id_fd, ERRAND_ID_IOC_COUNTERS);
	b = id_self ? ioctl(id_fd, ERRAND_ID_IOC_T6_VA, gate_fn) : ioctl(id_fd, ERRAND_ID_IOC_T6);
	for (i = 0; i < 10000; i++) {
		r = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40); /* the gate is no longer callable */
		if (r == 100) served++;
		else if ((long)r == -1) refused++;
		else other++;
	}
	printf("ipc_perf: T6 pid %d: module write to the gate page: %#lx; then 10000 calls: served %lu, refused %lu, other %lu\n",
	       getpid(), b, served, refused, other);
	printf("ipc_perf: T6 gate page bytes 0-15 after:");
	for (i = 0; i < 16; i++) printf(" %02x", g[i]);
	if (id_self) /* Phase 6: the revocation restored the process's own entry (the zero page) */
		printf("; all 4096 bytes zero (own entry back): %s\n", ({ int ok = 1; for (i = 0; i < 4096; i++) if (g[i]) ok = 0; ok ? "yes" : "NO"; }));
	else
		printf("; bytes 16-4095 all 0xcc: %s\n", ({ int ok = 1; for (i = 16; i < 4096; i++) if (g[i] != 0xcc) ok = 0; ok ? "yes" : "NO"; }));
	fflush(stdout);
	ioctl(id_fd, ERRAND_ID_IOC_COUNTERS);
}

static void
id_t11(unsigned long secs, int tamper)
{
	struct errand_id_t11 a;
	struct timespec      t0, t;
	unsigned long        n = 0, ok = 0, marked = 0, mismatch = 0, refused = 0, v, x;

	ioctl(id_fd, ERRAND_ID_IOC_COUNTERS); /* pong before */
	memset(&a, 0, sizeof(a));
	a.mode = ERRAND_ID_T11_COUNT | (tamper ? ERRAND_ID_T11_TAMPER : 0);
	if (ioctl(id_fd, ERRAND_ID_IOC_T11, &a)) printf("ipc_perf: T11 arm failed\n");
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (;;) {
		x = (n & 0xffffff) + 1;
		v = vm_call(VM_IPC_CAP_ENCODE(0), x, 0, 0, 0); /* pong_args: the sum of its arguments */
		n++;
		if (v == x) ok++;
		else if ((long)v == -1) refused++;
		else if (T11_MARKED(v)) marked++;
		else mismatch++;
		if ((n & 0xfff) == 0) {
			clock_gettime(CLOCK_MONOTONIC, &t);
			if ((unsigned long)(t.tv_sec - t0.tv_sec) >= secs) break;
		}
	}
	a.mode = 0;
	ioctl(id_fd, ERRAND_ID_IOC_T11, &a);
	printf("ipc_perf: T11 pid %d %s %lu s: %lu calls: right result %lu, served WITH THE MARKER %lu, other mismatch %lu, refused %lu\n",
	       getpid(), tamper ? "tamper" : "count", secs, n, ok, marked, mismatch, refused);
	printf("ipc_perf: T11 module: timer %lu, frames of this process in user mode %lu; in the gate by offset 0/1/7/10/11/16/other "
	       "%lu/%lu/%lu/%lu/%lu/%lu/%lu; blank %lu, not blank %lu (of which only the module's own marker %lu); tampered %lu\n",
	       a.ticks, a.frames, a.in_gate[0], a.in_gate[1], a.in_gate[2], a.in_gate[3], a.in_gate[4], a.in_gate[5],
	       a.in_gate[6], a.blank, a.nonblank, a.nonblank_own, a.tampered);
	fflush(stdout);
	ioctl(id_fd, ERRAND_ID_IOC_COUNTERS); /* pong after */
}

/*
 * 4.10 (A58): one timed slot-0 call through a copy of the stub that is entered by a direct jmp and
 * left by a direct jmp, so no ret runs between the vmcall and the second timestamp. Serialized as
 * rdtsc_serialized_start/end are (cpuid; rdtsc ... rdtscp; lfence). Returns the cycles; the call's
 * result goes to jmp_last_rax.
 */
static int cmp_u64(const void *a, const void *b);
unsigned long jmp_last_rax;
unsigned long bench_jmp_sample(void);
__asm__(".text\n.globl bench_jmp_sample\nbench_jmp_sample:\n\t"
        "pushq %rbx\n\tpushq %rbp\n\tpushq %r12\n\tpushq %r13\n\tpushq %r14\n\tpushq %r15\n\t"
        "xorl %eax, %eax\n\tcpuid\n\trdtsc\n\tshlq $32, %rdx\n\torq %rdx, %rax\n\tpushq %rax\n\t"
        "movq $0x410000, %rax\n\txorl %ecx, %ecx\n\tmovl $10, %ebx\n\tmovl $20, %esi\n\tmovl $30, %edi\n\tmovl $40, %edx\n\t"
        "jmp jstub_entry\n"
        "jstub_back:\n\t"
        "movq %rax, jmp_last_rax(%rip)\n\t"
        "rdtscp\n\tlfence\n\tshlq $32, %rdx\n\torq %rdx, %rax\n\t"
        "popq %rcx\n\tsubq %rcx, %rax\n\t"
        "popq %r15\n\tpopq %r14\n\tpopq %r13\n\tpopq %r12\n\tpopq %rbp\n\tpopq %rbx\n\t"
        "ret\n"
        "jstub_entry:\n\t"
        "pushq %rbp\n\tmovl $0x3202, %r11d\n\tvmcall\n\tpopq %rbp\n\tjmp jstub_back\n");

static void
id_stub_jmp(void)
{
	unsigned long i, bad = 0;

	for (i = 0; i < 100000; i++) {
		bench_pre0[i] = bench_jmp_sample();
		if (jmp_last_rax != 100) bad++;
	}
	qsort(bench_pre0, 100000, sizeof(bench_pre0[0]), cmp_u64);
	printf("ipc_perf: stub-jmp slot 0: 100000 samples, wrong results %lu\n", bad);
	printf("Min:            %lu\nMedian (50%%):   %lu.00\n", (unsigned long)bench_pre0[0], (unsigned long)bench_pre0[50000]);
	printf("  Slot 0 median: %lu.00 cycles\n", (unsigned long)bench_pre0[50000]);
	fflush(stdout);
}

static void
id_t12(void)
{
	unsigned long v, r;

	ioctl(id_fd, ERRAND_ID_IOC_COUNTERS);
	ioctl(id_fd, ERRAND_ID_IOC_T12, 0);
	v = vm_call(VM_IPC_CAP_ENCODE(0), 5, 0, 0, 0); /* single-stepped into the gate */
	r = (unsigned long)ioctl(id_fd, ERRAND_ID_IOC_T12, 1);
	printf("ipc_perf: T12 pid %d: single-stepped call with argument 5: result %#lx (%s); module: %lu steps, reached the vmcall %lu\n",
	       getpid(), v, v == 5 ? "the process's argument" : T11_MARKED(v) ? "SERVED WITH THE MARKER" : (long)v == -1 ? "refused" : "other",
	       r >> 8, r & 0xff);
	fflush(stdout);
	ioctl(id_fd, ERRAND_ID_IOC_COUNTERS);
}

static int
cmp_u64(const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;

	return x < y ? -1 : x > y;
}

static void
id_wake(unsigned long secs)
{
	static uint64_t late[200000];
	struct timespec next, now;
	unsigned long   n = 0, i, total = secs * 1000;

	if (total > 200000) total = 200000;
	clock_gettime(CLOCK_MONOTONIC, &next);
	for (i = 0; i < total; i++) {
		next.tv_nsec += 1000000;
		if (next.tv_nsec >= 1000000000) {
			next.tv_nsec -= 1000000000;
			next.tv_sec++;
		}
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
		clock_gettime(CLOCK_MONOTONIC, &now);
		late[n++] = (uint64_t)((now.tv_sec - next.tv_sec) * 1000000000L + (now.tv_nsec - next.tv_nsec));
	}
	qsort(late, n, sizeof(late[0]), cmp_u64);
	printf("ipc_perf: wake latency pid %d: %lu wake-ups of 1 ms, lateness p50 %.1f us, p99 %.1f us, max %.1f us\n", getpid(),
	       n, late[n / 2] / 1000.0, late[n * 99 / 100] / 1000.0, late[n - 1] / 1000.0);
	fflush(stdout);
}

static void
id_cpuid_cost(void)
{
	static uint64_t d[100000];
	unsigned int    a, b, c, e;
	int             i;

	for (i = 0; i < 100000; i++) {
		uint64_t t0 = rdtsc_serialized_start(), t1;

		a = 0; c = 0;
		__asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(e));
		t1   = rdtsc_serialized_end();
		d[i] = t1 - t0;
	}
	qsort(d, 100000, sizeof(d[0]), cmp_u64);
	printf("ipc_perf: user-mode cpuid round trip (with its rdtsc pair): min %lu p50 %lu p99 %lu\n",
	       (unsigned long)d[0], (unsigned long)d[50000], (unsigned long)d[99000]);
}

static void
id_t8(void)
{
	struct stat   st;
	unsigned long i, s0 = 0, s1 = 0;
	int           fd = open("/tmp/t8.ready", O_WRONLY | O_CREAT | O_APPEND, 0644), n = 0;

	if (fd >= 0) { if (write(fd, "x", 1) < 0) {} close(fd); }
	while ((stat("/tmp/t8.ready", &st) || st.st_size < 2) && n++ < 600) usleep(100000); /* both registered */
	for (i = 0; i < 10000; i++) {
		if (vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40) == 100) s0++;
		if (vm_call(VM_IPC_CAP_ENCODE(1), 42, 100, 5, 7) == 12) s1++;
	}
	printf("ipc_perf: T8 pid %d: both registered (%lu bytes): slot 0 served %lu of 10000, slot 1 served %lu of 10000\n",
	       getpid(), (unsigned long)st.st_size, s0, s1);
	fflush(stdout);
}

/*
 * T16 (Phase 6): who may ask. In a prepared, unregistered process, each attempt is followed by the
 * VMM's dump (its last refusal and "pages locked now"): the leaf from ring 0; gate addresses whose
 * leaf is writable, absent, in the upper half, or behind a 2 MiB page (the module writes that leaf:
 * the guest has neither THP nor hugetlbfs). Then a good registration, the same root again, and
 * 10^4 calls through the gate.
 */
static void
id_t16(void)
{
	struct errand_id_cpuid c;
	unsigned long          upper = 0xffffffff81000000UL, big, i, served = 0;
	void                  *good, *w, *none, *m, *second;
	long                   r, rp;

	good = errand_self_reserve(1);
	memset(&c, 0, sizeof(c));
	c.op      = ERRAND_ID_SELF_REGISTER;
	c.gate_va = (unsigned long)good;
	r         = ioctl(id_fd, ERRAND_ID_IOC_CPUID_RING0, &c);
	printf("ipc_perf: T16 (1) the leaf from ring 0, gate %p: ioctl %ld, answer %ld: %s\n", good, r, (long)c.eax,
	       (long)c.eax == -ERRAND_ID_EPERM ? "refused (-EPERM)" : "NOT REFUSED");
	fflush(stdout);
	errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0);

	w = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	((volatile char *)w)[0] = 1;
	r = errand_self_cpuid(ERRAND_ID_SELF_REGISTER, (unsigned long)w, 0);
	printf("ipc_perf: T16 (2) a writable leaf at %p: %ld: %s\n", w, r, r == -ERRAND_ID_EINVAL ? "refused (-EINVAL)" : "NOT REFUSED");
	fflush(stdout);
	errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0);

	none = mmap(0, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	r    = errand_self_cpuid(ERRAND_ID_SELF_REGISTER, (unsigned long)none, 0);
	printf("ipc_perf: T16 (3) an absent leaf at %p (PROT_NONE, never populated): %ld: %s\n", none, r,
	       r == -ERRAND_ID_EINVAL ? "refused (-EINVAL)" : "NOT REFUSED");
	fflush(stdout);
	errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0);

	r = errand_self_cpuid(ERRAND_ID_SELF_REGISTER, upper, 0);
	printf("ipc_perf: T16 (4) the upper half, %#lx: %ld: %s\n", upper, r, r == -ERRAND_ID_EINVAL ? "refused (-EINVAL)" : "NOT REFUSED");
	fflush(stdout);
	errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0);

	m   = mmap(0, 4 << 20, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	big = ((unsigned long)m + (2UL << 20) - 1) & ~((2UL << 20) - 1);
	rp  = ioctl(id_fd, ERRAND_ID_IOC_T16_PMD, big);
	r   = rp ? 1 : errand_self_cpuid(ERRAND_ID_SELF_REGISTER, big, 0);
	printf("ipc_perf: T16 (5) behind a 2 MiB leaf at %#lx (module: %ld): %ld: %s\n", big, rp, r,
	       rp ? "NOT CONSTRUCTED" : r == -ERRAND_ID_EINVAL ? "refused (-EINVAL)" : "NOT REFUSED");
	fflush(stdout);
	errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0);
	if (!rp) printf("ipc_perf: T16 the 2 MiB leaf removed: %d\n", ioctl(id_fd, ERRAND_ID_IOC_T16_PMD, big | 1));

	/* reserved before the registration: an mmap after it writes the tables and revokes (A79) */
	second = errand_self_reserve(1);
	r      = errand_self_cpuid(ERRAND_ID_SELF_REGISTER, (unsigned long)good, 0);
	if (!r) {
		gate_fn = (unsigned long)good;
		id_self = 1;
	}
	rp     = errand_self_cpuid(ERRAND_ID_SELF_REGISTER, (unsigned long)second, 0);
	for (i = 0; i < 10000 && gate_fn; i++) if (vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40) == 100) served++;
	printf("ipc_perf: T16 (6) a good registration at %p: %ld; the same root again at %p: %ld: %s; then 10000 calls: served %lu\n",
	       good, r, second, rp, rp == -ERRAND_ID_EEXIST ? "refused (-EEXIST)" : "NOT REFUSED", served);
	fflush(stdout);
	errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0);
}

static int
id_main(int argc, char **argv, int *bench)
{
	int i, reg = 0, unreg = 0, want12 = 0, hold = 0, locked = 0, t4 = 0, churn = 0, gate = -1, t5 = 0, t6 = 0, t8 = 0;
	int stub_local = 0, t11_tamper = 0, t12 = 0, stub_jmp = 0, no_touch = 0, t16 = 0, dump_after = 0;
	unsigned long t11_secs = 0;
	unsigned long ainline = 0;
	unsigned long attempts = 0, kvm = 0, lflags = 0, heap_mb = 0, secs = 0;
	long r;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--register")) reg = 1;
		else if (!strcmp(argv[i], "--no-register")) unreg = 1;
		else if (!strcmp(argv[i], "--want-bit12")) want12 = 1;
		else if (!strcmp(argv[i], "--hold")) hold = 1;
		else if (!strcmp(argv[i], "--attempts") && i + 1 < argc) attempts = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--kernel-vmcall") && i + 1 < argc) kvm = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--register-locked")) locked = 1;
		else if (!strcmp(argv[i], "--no-preset")) lflags |= ERRAND_ID_LOCK_NO_PRESET;
		else if (!strcmp(argv[i], "--heap-mb") && i + 1 < argc) heap_mb = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) secs = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--t4")) t4 = 1;
		else if (!strcmp(argv[i], "--exit-registered")) id_exit_registered = 1;
		else if (!strcmp(argv[i], "--churn")) churn = 1;
		else if (!strcmp(argv[i], "--no-cpuid")) id_no_cpuid = 1;
		else if (!strcmp(argv[i], "--gate") && i + 1 < argc) gate = (int)strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--self") && i + 1 < argc) { gate = (int)strtoul(argv[++i], 0, 0); id_self = 1; }
		else if (!strcmp(argv[i], "--no-touch")) no_touch = 1;
		else if (!strcmp(argv[i], "--dump-after")) dump_after = 1;
		else if (!strcmp(argv[i], "--t16")) { t16 = 1; gate = 0; id_self = 1; }
		else if (!strcmp(argv[i], "--kernel-op") && i + 1 < argc) {
			struct errand_id_kvmcall k = { .rax = strtoul(argv[++i], 0, 0), .count = 1 };

			if (id_open() < 0) return 1;
			r = ioctl(id_fd, ERRAND_ID_IOC_KERNEL_VMCALL, &k);
			printf("ipc_perf: ring-0 operation %#lx from the module: answer %ld: %s\n", k.rax, (long)k.last_rax,
			       k.n_enosys == 1 ? "-ENOSYS (removed)" : "NOT -ENOSYS");
			*bench = 0;
		}
		else if (!strcmp(argv[i], "--gate-drop")) lflags |= ERRAND_ID_LOCK_DROP_ONE;
		else if (!strcmp(argv[i], "--attempts-inline") && i + 1 < argc) ainline = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--t5")) t5 = 1;
		else if (!strcmp(argv[i], "--t6")) t6 = 1;
		else if (!strcmp(argv[i], "--t8")) t8 = 1;
		else if (!strcmp(argv[i], "--stub-local")) stub_local = 1;
		else if (!strcmp(argv[i], "--t11") && i + 1 < argc) t11_secs = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--t11-tamper")) t11_tamper = 1;
		else if (!strcmp(argv[i], "--t12")) t12 = 1;
		else if (!strcmp(argv[i], "--stub-jmp")) stub_jmp = 1;
		else if (!strcmp(argv[i], "--cpuid-cost")) { id_cpuid_cost(); *bench = 0; }
		else if (!strcmp(argv[i], "--wake") && i + 1 < argc) { id_wake(strtoul(argv[++i], 0, 0)); *bench = 0; }
		else if (!strcmp(argv[i], "--dump")) {
			if (id_open_quiet() < 0) {
				r = errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0); /* Phase 6: no module in the image */
				printf("ipc_perf: dump requested (cpuid): %ld\n", r);
			} else {
				r = ioctl(id_fd, ERRAND_ID_IOC_DUMP);
				printf("ipc_perf: dump requested: %ld\n", r);
			}
			*bench = 0;
		} else if (!strcmp(argv[i], "--soak-fork") && i + 1 < argc) {
			*bench = 0;
			if (soak_fork(strtoul(argv[++i], 0, 0))) return 1;
		} else if (!strcmp(argv[i], "--soak-loop") && i + 1 < argc) {
			*bench = 0;
			if (soak_loop(strtoul(argv[++i], 0, 0))) return 1;
		} else if (!strcmp(argv[i], "--counters")) {
			if (id_open_quiet() < 0) {
				r = errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0); /* Phase 6: no module; the dump holds the counters too */
				printf("ipc_perf: counters requested (cpuid): %ld\n", r);
			} else {
				r = ioctl(id_fd, ERRAND_ID_IOC_COUNTERS);
				printf("ipc_perf: counters requested: %ld\n", r);
			}
			*bench = 0;
		} else {
			printf("ipc_perf: unknown argument %s\n", argv[i]);
			return 1;
		}
	}
	if (churn) {
		unsigned long rounds = 0;

		for (;;) {
			char *m = mmap(0, 16 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

			if (m == MAP_FAILED) continue;
			memset(m, 1, 16 << 20);
			munmap(m, 16 << 20);
			if ((++rounds & 3) == 0) {
				char b[32];
				int  fd = open("/tmp/churn.cnt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (fd >= 0) {
					write(fd, b, snprintf(b, sizeof(b), "%lu\n", rounds));
					close(fd);
				}
			}
		}
	}
	if (locked || gate >= 0) {
		struct errand_id_reg_stat st = { 0 };
		unsigned long             root;
		void                     *g = NULL;

		if (id_self) id_open_quiet(); /* Phase 6: the module, if any, is only the attacker */
		else if (id_open() < 0) return 1;
		root = id_root();
		if (gate >= 0 && !id_self) {
			/* the gate page, mapped before the tables are walked and locked */
			g = mmap(0, 4096, PROT_READ | PROT_EXEC, MAP_SHARED, id_fd, 0);
			if (g == MAP_FAILED) {
				printf("ipc_perf: gate mmap failed\n");
				return 1;
			}
		}
		if (gate >= 0) {
			memset(t5_stack, 0, sizeof(t5_stack)); /* T5's data, present before registering */
			t5_saved_rsp = t5_rax = 0;
		}
		lock_prepare(heap_mb, (*bench && !attempts && !secs && !t4 && !t5 && !t6 && !t8 && !ainline && !t11_secs && !t12) || stub_jmp);
		if (t16) {
			id_t16();
			*bench = 0;
			return 0;
		}
		if (id_self) {
			/* Phase 6 (A71, A76): this process asks the VMM itself; T9's cycles are around the cpuid alone */
			void    *gp = errand_self_reserve(!no_touch);
			uint64_t t0, t1;

			if (!gp) {
				printf("ipc_perf: pid %d: mlockall or the gate's mmap failed\n", getpid());
				return 1;
			}
			t0 = rdtsc_serialized_start();
			r  = errand_self_cpuid(ERRAND_ID_SELF_REGISTER, (unsigned long)gp, (unsigned long)gate);
			t1 = rdtsc_serialized_end();
			if (!r) gate_fn = (unsigned long)gp;
			printf("ipc_perf: pid %d registered itself (cpuid) root %#lx with a gate at %p, component %d%s: %ld (%lu cycles around the cpuid)\n",
			       getpid(), root, gp, gate, no_touch ? " (gate address never read before)" : " (gate address read before)", r,
			       (unsigned long)(t1 - t0));
			fflush(stdout);
			if (!r && id_fd >= 0) ioctl(id_fd, ERRAND_ID_IOC_GATE_VA, gate_fn); /* where the attacker looks (T11, T12) */
			if (dump_after) errand_self_cpuid(ERRAND_ID_SELF_DUMP, 0, 0);
		} else if (gate >= 0) {
			struct errand_id_gate_req req = { (unsigned long)g, (unsigned long)gate, lflags };

			r = ioctl(id_fd, ERRAND_ID_IOC_REGISTER_GATE, &req);
			if (!r) gate_fn = (unsigned long)g;
		} else {
			r = ioctl(id_fd, ERRAND_ID_IOC_REGISTER_LOCKED, lflags);
		}
		if (!id_self && r == -1) r = -errno; /* the module's answer */
		if (!id_self) {
			ioctl(id_fd, ERRAND_ID_IOC_LAST_REG, &st);
			printf("ipc_perf: pid %d registered %s root %#lx%s: %ld (%lu page-table pages, %lu cycles)\n", getpid(),
			       gate >= 0 ? "with a gate" : "locked", root, (lflags & ERRAND_ID_LOCK_NO_PRESET) ? " (no preset)" : "",
			       r, st.pages, st.cycles);
			fflush(stdout);
		}
		if (r) return 1;
		if (stub_local && gate < 0) gate_fn = (unsigned long)local_stub;
		if (ainline) {
			unsigned long k, sv = 0, rf = 0, ot = 0, v;

			for (k = 0; k < ainline; k++) {
				v = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
				if (v == 100) sv++;
				else if ((long)v == -1) rf++;
				else ot++;
			}
			printf("ipc_perf: inline (outside the gate) pid %d: %lu calls, served %lu, refused %lu, other %lu\n", getpid(),
			       ainline, sv, rf, ot);
			*bench = 0;
		}
		if (t5) { id_t5(); *bench = 0; }
		if (t6) { id_t6(); *bench = 0; }
		if (t8) { id_t8(); *bench = 0; }
		if (t11_secs) { id_t11(t11_secs, t11_tamper); *bench = 0; }
		if (t12) { id_t12(); *bench = 0; }
		if (stub_jmp) { id_check(); id_stub_jmp(); *bench = 0; }
		if (t4) {
			id_t4();
			*bench = 0;
		}
		if (secs) {
			id_seconds(secs);
			*bench = 0;
		}
	}
	if (reg) {
		if (id_open() < 0) return 1;
		r = ioctl(id_fd, ERRAND_ID_IOC_REGISTER);
		printf("ipc_perf: pid %d registered root %#lx: %ld\n", getpid(), id_root(), r);
		if (r) return 1;
		if (secs) { /* H1: the T3 loop with a plain, unlocked registration */
			pin_cpu0();
			id_seconds(secs);
			*bench = 0;
		}
	}
	if (unreg) {
		unsigned long root = id_root();

		printf("ipc_perf: pid %d unregistered, root %#lx (bit 12 %s)\n", getpid(), root,
		       (root & 0x1000) ? "set" : "clear");
		if (want12 && !(root & 0x1000)) return 3;
	}
	if (kvm) {
		struct errand_id_kvmcall k = { .rax = VM_IPC_CAP_ENCODE(0), .count = kvm };

		if (id_open() < 0) return 1;
		r = ioctl(id_fd, ERRAND_ID_IOC_KERNEL_VMCALL, &k);
		printf("ipc_perf: ring-0 vmcalls rax %#lx: %lu calls, refused (-ENOSYS) %lu, other %lu (last %ld): %ld\n",
		       k.rax, k.count, k.n_enosys, k.n_other, (long)k.last_rax, r);
		*bench = 0;
	}
	if (attempts) {
		pin_cpu0();
		id_attempts(attempts);
		*bench = 0;
	}
	if (hold) {
		signal(SIGTERM, id_on_term);
		printf("ipc_perf: pid %d HOLDING its registration\n", getpid());
		fflush(stdout);
		while (!id_stop) pause();
	}
	return 0;
}

static void
id_exit(void)
{
	long r;

	if (id_self) {
		/* Phase 6: unregister with the leaf; the VMM puts the process's own entry back (the zero page) */
		if (id_exit_registered) {
			printf("ipc_perf: pid %d exits without unregistering\n", getpid());
			fflush(stdout);
			return;
		}
		r = errand_self_unregister();
		printf("ipc_perf: pid %d unregistered itself: %ld\n", getpid(), r);
		if (gate_fn) {
			unsigned char *g = (unsigned char *)gate_fn;
			int            i, z = 1;

			for (i = 0; i < 4096; i++) if (g[i]) z = 0;
			printf("ipc_perf: pid %d gate address after unregister: %02x %02x %02x ... all 4096 bytes zero (own entry back): %s\n",
			       getpid(), g[0], g[1], g[2], z ? "yes" : "NO");
		}
		fflush(stdout);
		return;
	}
	if (id_fd < 0) return;
	if (id_exit_registered) {
		printf("ipc_perf: pid %d exits without unregistering\n", getpid());
		fflush(stdout);
		return;
	}
	r = ioctl(id_fd, ERRAND_ID_IOC_UNREGISTER);
	printf("ipc_perf: pid %d unregistered: %ld\n", getpid(), r);
	if (gate_fn) {
		unsigned char *g = (unsigned char *)gate_fn;
		int            i, cc = 1;

		for (i = 0; i < 4096; i++) if (g[i] != 0xcc) cc = 0;
		printf("ipc_perf: pid %d gate page after unregister: %02x %02x %02x ... all 4096 bytes 0xcc: %s\n", getpid(),
		       g[0], g[1], g[2], cc ? "yes" : "NO");
	}
}

static void ipc_perf_bench(void);

int main(int argc, char **argv)
{
	int bench = 1, reg = 0, i;

	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "--register") || !strcmp(argv[i], "--register-locked") || !strcmp(argv[i], "--gate")
		    || !strcmp(argv[i], "--self") || !strcmp(argv[i], "--t16")) reg = 1;
	if (id_main(argc, argv, &bench)) return 1;
	if (bench) {
		if (argc > 1 && id_check()) {
			if (reg) id_exit();
			return 1;
		}
		ipc_perf_bench();
	}
	if (reg) id_exit();
	return 0;
}

static void ipc_perf_bench(void)
{
	const int N = 100000;  /* Number of iterations for each benchmark */
	unsigned long ret0, ret1, ret2;

	printf("\n");
	printf("╔═══════════════════════════════════════════════════════════╗\n");
	printf("║      VM IPC Performance Benchmark - vmcall Latency       ║\n");
	printf("╚═══════════════════════════════════════════════════════════╝\n");
	printf("\n");
	printf("Number of iterations per benchmark: %d\n", N);
	printf("Pinning to CPU 0 and locking memory for stable measurements...\n");
	
	/* Pin to CPU 0 and lock memory for stable measurements */
	pin_cpu0();
	lock_mem();

	/* Allocate buffers for sample collection (a locked registration made them before registering) */
	uint64_t *samples_slot0 = bench_pre0 ? bench_pre0 : aligned_alloc(64, N * sizeof(uint64_t));
	uint64_t *samples_slot1 = bench_pre1 ? bench_pre1 : aligned_alloc(64, N * sizeof(uint64_t));

	if (!samples_slot0 || !samples_slot1) {
		printf("Error: Failed to allocate memory for samples\n");
		return;
	}

	/* Initialize statistics structures */
	stats_t stats_slot0, stats_slot1;
	stats_init(&stats_slot0, samples_slot0, N);
	stats_init(&stats_slot1, samples_slot1, N);

	printf("\n");
	printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
	printf("Benchmark 1: vmcall Slot 0 - pong_args(10,20,30,40)\n");
	printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
	printf("Running %d iterations...\n", N);

	/* Benchmark Slot 0: pong_args - simple 4-argument function */
	for (int i = 0; i < N; i++) {
		uint64_t t0 = rdtsc_serialized_start();
		ret0 = vm_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
		uint64_t t1 = rdtsc_serialized_end();
		stats_add_sample(&stats_slot0, t1 - t0);
	}

	stats_print_detailed(&stats_slot0, "vmcall Slot 0: pong_args(10,20,30,40)");

	printf("\n");
	printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
	printf("Benchmark 2: vmcall Slot 1 - pong_argsrets(42,100,5,7)\n");
	printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
	printf("Running %d iterations...\n", N);

	/* Benchmark Slot 1: pong_argsrets - function with multiple return values */
	for (int i = 0; i < N; i++) {
		uint64_t t0 = rdtsc_serialized_start();
		ret0 = vm_call(VM_IPC_CAP_ENCODE(1), 42, 100, 5, 7);
		uint64_t t1 = rdtsc_serialized_end();
		stats_add_sample(&stats_slot1, t1 - t0);
	}

	stats_print_detailed(&stats_slot1, "vmcall Slot 1: pong_argsrets(42,100,5,7)");

	printf("\n");
	printf("╔═══════════════════════════════════════════════════════════╗\n");
	printf("║                   Benchmark Complete                      ║\n");
	printf("╚═══════════════════════════════════════════════════════════╝\n");
	printf("\n");
	printf("[vm_ipc_perf] Comparison:\n");
	printf("  Slot 0 median: %.2f cycles\n", stats_median(&stats_slot0));
	printf("  Slot 1 median: %.2f cycles\n", stats_median(&stats_slot1));
	printf("  Difference:    %.2f cycles (%.2f%%)\n", 
	       stats_median(&stats_slot1) - stats_median(&stats_slot0),
	       100.0 * (stats_median(&stats_slot1) - stats_median(&stats_slot0)) / stats_median(&stats_slot0));
	printf("\n");

	/* Cleanup */
	if (!bench_pre0) { /* a locked process frees nothing: the unmap would revoke it */
		stats_free(&stats_slot0);
		stats_free(&stats_slot1);
	}
	free(samples_slot0);
	free(samples_slot1);

	printf("[vm_ipc_perf] DONE\n\n");
}
