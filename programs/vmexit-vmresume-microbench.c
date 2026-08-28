#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

// Define the IOCTL commands (must match the kernel module)
#define IOCTL_RUN_VMCALL   _IOW('v', 1, unsigned long)  // runs N vmcall
#define IOCTL_RUN_FAST     _IOW('v', 2, unsigned long)  // runs N cpuid (fast path)
#define IOCTL_RUN_SLOW     _IOW('v', 3, unsigned long)  // runs N out 0xE9 from kernel

#define DEVICE_PATH "/dev/kvm-microbench"

int main(int argc, char *argv[]) {
    int fd;
    unsigned long num_iterations = 10000;  // Default value
    int ret;

    // Parse number of iterations if provided
    if (argc >= 2) {
        num_iterations = strtoul(argv[1], NULL, 10);
        if (num_iterations == 0) {
            fprintf(stderr, "Error: Invalid number of iterations\n");
            printf("Usage: %s [num_iterations]\n", argv[0]);
            printf("  num_iterations: Number of samples to collect (default: 200000)\n");
            return 1;
        }
    }

    printf("=== VMEXIT-VMRESUME Microbenchmark ===\n");
    printf("Number of iterations: %lu\n\n", num_iterations);

    // Open the device
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open device %s: %s\n", 
                DEVICE_PATH, strerror(errno));
        fprintf(stderr, "Make sure the kernel module is loaded.\n");
        fprintf(stderr, "Try: sudo insmod modules/mesurement-module.ko\n");
        return 1;
    }

    printf("Device opened successfully.\n\n");
    
    // Trigger the microbenchmark tests via vmcall
    printf("Running VMCALL test in kernel...\n");
    ret = ioctl(fd, IOCTL_RUN_VMCALL, num_iterations);
    if (ret < 0) {
        fprintf(stderr, "Error: IOCTL_RUN_VMCALL failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    printf("  ✓ Completed\n\n");

    close(fd);
    return 0;
}