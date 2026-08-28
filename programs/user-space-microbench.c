#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/io.h>
#include "stats.h"

static inline uint64_t rdtsc_serialized_start(void) {
    unsigned int a, d;
    // CPUID;RDTSC is nicely serialized
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

static void pin_cpu0(void) {
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

static void lock_mem(void) { mlockall(MCL_CURRENT|MCL_FUTURE); }

int main(int argc, char **argv) {
    const int N = (argc>1)?atoi(argv[1]):500000;
    //pin_cpu0(); lock_mem();

    printf("=== User Space Microbenchmark ===\n");
    printf("Number of iterations: %d\n\n", N);

    uint64_t *s1 = aligned_alloc(64, N*sizeof(uint64_t));
    uint64_t *s2 = aligned_alloc(64, N*sizeof(uint64_t));

    stats_t stats1, stats2;
    stats_init(&stats1, s1, N);
    stats_init(&stats2, s2, N);

    // FAST PATH: CPUID (handled in KVM kernel)
    for (int i=0;i<N;i++) {
        uint64_t t0 = rdtsc_serialized_start();
        int ax=0x0, bx, cx, dx;
        asm volatile("cpuid":"+a"(ax), "=b"(bx), "=c"(cx), "=d"(dx));
        uint64_t t1 = rdtsc_serialized_end();
        stats_add_sample(&stats1, t1 - t0);
    }

    // SLOW PATH: outb to port 0xE9 (handled in QEMU userspace)
    if (ioperm(0xE9, 1, 1)) {
        perror("ioperm");
        fprintf(stderr, "Warning: Cannot get I/O port access. Skipping slow path test.\n");
        fprintf(stderr, "This requires CONFIG_X86_IOPL_IOPERM=y in kernel config.\n");
        stats_print_detailed(&stats1, "CPUID(user, fast)");
        return 0;
    }
    for (int i=0;i<N;i++) {
        uint64_t t0 = rdtsc_serialized_start();
        __asm__ __volatile__ (
	        "outb %b0, %w1":: "a"('T'), "Nd"(0xe9) : "memory");
        uint64_t t1 = rdtsc_serialized_end();
        stats_add_sample(&stats2, t1 - t0);
    }

    stats_print_detailed(&stats1, "CPUID(user, fast)");
    stats_print_detailed(&stats2, "OUT 0xE9(user, slow)");
    return 0;
}
