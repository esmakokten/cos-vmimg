/*
 * exitbench_abi.h -- ioctl ABI shared between the guest kernel module and the
 * userspace driver. Included from both, so it may use nothing but fixed-width
 * types and <linux/ioctl.h> / <sys/ioctl.h>.
 */
#ifndef EXITBENCH_ABI_H
#define EXITBENCH_ABI_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint64_t __u64;
typedef uint32_t __u32;
#endif

#define EXITBENCH_DEVICE "/dev/exit-bench"

/*
 * The rungs. Each is one named exit type; the ladder is only meaningful if
 * every rung is confirmed to take the path its name claims -- see the path gate
 * in ../host/run.sh (trace-cmd on kvm:kvm_exit / kvm:kvm_userspace_exit).
 */
enum exitbench_rung {
	RUNG_L0_RDTSC        = 0, /* no exit at all: harness overhead            */
	RUNG_L1_FASTPATH_MSR = 1, /* WRMSR IA32_TSC_DEADLINE: KVM fastpath       */
	RUNG_L2_VMCALL       = 2, /* VMCALL, unknown nr: kvm_emulate_hypercall   */
	RUNG_L3_CPUID        = 3, /* CPUID leaf 0: kvm_emulate_cpuid             */
	RUNG_L4_RDMSR        = 4, /* RDMSR UCODE_REV: intercepted MSR read       */
	RUNG_L5_HLT          = 5, /* HLT with an interrupt already pending       */
	RUNG_L6_PIO          = 6, /* OUT 0xE9: userspace exit, KVM_EXIT_IO       */
	RUNG_L7_MMIO         = 7, /* store to unbacked GPA: KVM_EXIT_MMIO        */
	RUNG_MAX
};

/* flags */
#define EXITBENCH_F_COLD   (1u << 0) /* pollute caches/BP between samples     */

struct exitbench_req {
	__u32 rung;
	__u32 flags;
	__u64 n;          /* number of samples                                   */
	__u64 chunk;      /* samples per irq-disabled window (0 -> default 1024) */
	__u64 mmio_gpa;   /* RUNG_L7 target; 0 -> module picks one above RAM     */
	__u64 samples;    /* user pointer to __u64[n], filled on return          */
	__u64 mmio_gpa_used; /* out: the GPA actually used                       */
};

#define EXITBENCH_IOC_MAGIC 'x'
#define EXITBENCH_RUN   _IOWR(EXITBENCH_IOC_MAGIC, 1, struct exitbench_req)

#endif /* EXITBENCH_ABI_H */
