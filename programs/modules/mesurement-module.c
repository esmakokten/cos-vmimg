#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/msr.h>
#include <asm/processor.h>

#include <linux/version.h>

/*
 * class_create() lost its owner argument in 6.4 (commit 1aaba11da9aa,
 * "driver core: class: remove module * from class_create()").
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
#define COS_CLASS_CREATE(name) class_create(name)
#else
#define COS_CLASS_CREATE(name) class_create(THIS_MODULE, name)
#endif

static dev_t devno;
static struct cdev cdev;
static struct class *cls;

static inline u64 tsc_start(void){
    unsigned int a,d;
    asm volatile("cpuid" : : "a"(0) : "rbx","rcx","rdx");
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    return ((u64)d<<32)|a;
}
static inline u64 tsc_end(void){
    unsigned int a,d,c;
    asm volatile("rdtscp" : "=a"(a), "=d"(d), "=c"(c));
    asm volatile("lfence");
    return ((u64)d<<32)|a;
}

#define IOCTL_RUN_VMCALL   _IOW('v', 1, unsigned long)  // runs N vmcall
#define IOCTL_RUN_FAST     _IOW('v', 2, unsigned long)  // runs N vmcall
#define IOCTL_RUN_SLOW     _IOW('v', 3, unsigned long)  // runs N out 0xE9 from kernel

static u64 *samples;
static size_t S;

static int cmp_u64(const void *a, const void *b)
{
    u64 x = *(const u64 *)a;
    u64 y = *(const u64 *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static long dev_ioctl(struct file *f, unsigned int cmd, unsigned long arg){
    size_t N = arg ? arg : 200000;
    if (!samples || S < N) {
        kfree(samples);
        samples = kmalloc_array(N, sizeof(u64), GFP_KERNEL);
        if (!samples)
            return -ENOMEM;
        S = N;
    }
    size_t i;

    // Print command as a string
    const char *cmd_str = "unknown";
    switch (cmd) {
    case IOCTL_RUN_VMCALL: cmd_str = "IOCTL_RUN_VMCALL"; break;
    case IOCTL_RUN_FAST: cmd_str = "IOCTL_RUN_FAST"; break;
    case IOCTL_RUN_SLOW: cmd_str = "IOCTL_RUN_SLOW"; break;
    }
    printk(KERN_INFO "kvm-microbench: ioctl cmd=%s N=%zu\n", cmd_str, N);

    switch (cmd) {
    case IOCTL_RUN_FAST:
        for (i=0;i<N;i++) {
            u64 t0 = tsc_start();
            int ax=0x0, bx, cx, dx;
            asm volatile("cpuid":"+a"(ax), "=b"(bx), "=c"(cx), "=d"(dx));
            u64 t1 = tsc_end();
            samples[i] = t1 - t0;
        }
        break;
    case IOCTL_RUN_VMCALL:
        for (i=0;i<N;i++) {
            u64 t0 = tsc_start();
            asm volatile("vmcall" ::: "memory");
            u64 t1 = tsc_end();
            samples[i] = t1 - t0;
        }
        break;
    case IOCTL_RUN_SLOW:
        for (i=0;i<N;i++) {
            u64 t0 = tsc_start();
            asm volatile("outb %b0, %w1":: "a"('T'), "Nd"(0xe9) : "memory");
            u64 t1 = tsc_end();
            samples[i] = t1 - t0;
        }
        break;
    default: return -EINVAL;
    }

    // compute p50,p90,p99
    sort(samples, N, sizeof(u64), cmp_u64, NULL);    
    u64 p50 = samples[(N * 50) / 100];
    u64 p90 = samples[(N * 90) / 100];
    u64 p99 = samples[(N * 99) / 100];
    // calculate average
    u64 sum = 0;
    for (i = 0; i < N; i++) {
        sum += samples[i];
    }
    u64 avg = sum / N;
    // max min
    u64 max = samples[N - 1];
    u64 min = samples[0];
    
    printk(KERN_INFO "Microbench results over %zu samples: min=%llu max=%llu avg=%llu p50=%llu p90=%llu p99=%llu\n",
           N, (unsigned long long)min, (unsigned long long)max, (unsigned long long)avg,
           (unsigned long long)p50, (unsigned long long)p90, (unsigned long long)p99);

    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = dev_ioctl,
};

static int __init microbench_init(void){
    int ret;
    ret = alloc_chrdev_region(&devno, 0, 1, "kvm-microbench");
    if (ret) return ret;

    cdev_init(&cdev, &fops);
    ret = cdev_add(&cdev, devno, 1);
    if (ret) goto err_unregister;

    cls = COS_CLASS_CREATE("kvm-microbench");
    if (IS_ERR(cls)) { ret = PTR_ERR(cls); goto err_cdev; }

    if (!device_create(cls, NULL, devno, NULL, "kvm-microbench")) {
        ret = -ENOMEM; goto err_class;
    }

    samples = NULL;
    S = 0;

    printk(KERN_INFO "kvm-microbench module loaded\n");
    return 0;

err_class:
    class_destroy(cls);
err_cdev:
    cdev_del(&cdev);
err_unregister:
    unregister_chrdev_region(devno, 1);
    return ret;
}

static void __exit microbench_exit(void){
    device_destroy(cls, devno);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(devno, 1);
    kfree(samples);
    printk(KERN_INFO "kvm-microbench module unloaded\n");
}

module_init(microbench_init);
module_exit(microbench_exit);
MODULE_LICENSE("GPL");