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
#include "stats.h"
#include "errand_id_abi.h"

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
	                     : "=a"(ret), "=c"(fault)
	                     : "a"(cap_no), "b"(arg1), "S"(arg2), "D"(arg3), "d"(arg4), "c"(0)
	                     : "memory", "cc", "r8", "r9", "r11","r12");
	return ret;
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
 * With no flag, ipc_perf is the T0 benchmark it always was.
 */
static int id_fd = -1;
static volatile sig_atomic_t id_stop;

static void id_on_term(int sig) { (void)sig; id_stop = 1; }

static int
id_open(void)
{
	if (id_fd < 0) id_fd = open("/dev/errand_id", O_RDWR);
	if (id_fd < 0) printf("ipc_perf: cannot open /dev/errand_id\n");
	return id_fd;
}

static unsigned long
id_root(void)
{
	unsigned long root = 0;

	if (id_open() >= 0 && ioctl(id_fd, ERRAND_ID_IOC_ROOT, &root)) root = 0;
	return root;
}

/* 1,000 checked calls per slot before anything is timed: the correctness gate on the real path. */
static int
id_check(void)
{
	int i, bad0 = 0, bad1 = 0;

	for (i = 0; i < 1000; i++) {
		if (vm_ipc_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40) != 100) bad0++;
		if (vm_ipc_call(VM_IPC_CAP_ENCODE(1), 42, 100, 5, 7) != 12) bad1++;
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
		last = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
		if (last == 100) served++;
		else if ((long)last == -1 /* -EPERM */) refused++;
		else other++;
	}
	printf("ipc_perf: attempts pid %d root %#lx: %lu calls, served %lu, refused %lu, other %lu (last %ld)\n",
	       getpid(), id_root(), n, served, refused, other, (long)last);
}

static int
id_main(int argc, char **argv, int *bench)
{
	int i, reg = 0, unreg = 0, want12 = 0, hold = 0;
	unsigned long attempts = 0, kvm = 0;
	long r;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--register")) reg = 1;
		else if (!strcmp(argv[i], "--no-register")) unreg = 1;
		else if (!strcmp(argv[i], "--want-bit12")) want12 = 1;
		else if (!strcmp(argv[i], "--hold")) hold = 1;
		else if (!strcmp(argv[i], "--attempts") && i + 1 < argc) attempts = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--kernel-vmcall") && i + 1 < argc) kvm = strtoul(argv[++i], 0, 0);
		else if (!strcmp(argv[i], "--counters")) {
			if (id_open() < 0) return 1;
			r = ioctl(id_fd, ERRAND_ID_IOC_COUNTERS);
			printf("ipc_perf: counters requested: %ld\n", r);
			*bench = 0;
		} else {
			printf("ipc_perf: unknown argument %s\n", argv[i]);
			return 1;
		}
	}
	if (reg) {
		if (id_open() < 0) return 1;
		r = ioctl(id_fd, ERRAND_ID_IOC_REGISTER);
		printf("ipc_perf: pid %d registered root %#lx: %ld\n", getpid(), id_root(), r);
		if (r) return 1;
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

	if (id_fd < 0) return;
	r = ioctl(id_fd, ERRAND_ID_IOC_UNREGISTER);
	printf("ipc_perf: pid %d unregistered: %ld\n", getpid(), r);
}

static void ipc_perf_bench(void);

int main(int argc, char **argv)
{
	int bench = 1, reg = 0, i;

	for (i = 1; i < argc; i++) if (!strcmp(argv[i], "--register")) reg = 1;
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

	/* Allocate buffers for sample collection */
	uint64_t *samples_slot0 = aligned_alloc(64, N * sizeof(uint64_t));
	uint64_t *samples_slot1 = aligned_alloc(64, N * sizeof(uint64_t));

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
		ret0 = vm_ipc_call(VM_IPC_CAP_ENCODE(0), 10, 20, 30, 40);
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
		ret0 = vm_ipc_call(VM_IPC_CAP_ENCODE(1), 42, 100, 5, 7);
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
	stats_free(&stats_slot0);
	stats_free(&stats_slot1);
	free(samples_slot0);
	free(samples_slot1);

	printf("[vm_ipc_perf] DONE\n\n");
}
