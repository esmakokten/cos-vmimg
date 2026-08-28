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

static dev_t devno;
static struct cdev cdev;
static struct class *cls;

#define IOCTL_RUN_VMCALL   _IOW('v', 1, unsigned long)  // does vmcall
#define IOCTL_RUN_CPUID     _IOW('v', 2, unsigned long)  // does cpuid
#define IOCTL_RUN_OUTB     _IOW('v', 3, unsigned long)  // does out 0xE9 from kernel
#define IOCTL_RUN_EMPTY    _IOW('v', 4, unsigned long)  // does nothing

static long dev_ioctl(struct file *f, unsigned int cmd, unsigned long arg){

    switch (cmd) {
        case IOCTL_RUN_CPUID: {
            int ax=0x0, bx, cx, dx;
            asm volatile("cpuid":"+a"(ax), "=b"(bx), "=c"(cx), "=d"(dx));
            break;
        }
        case IOCTL_RUN_VMCALL: {
            asm volatile("vmcall" ::: "memory");
            break;
        }
        case IOCTL_RUN_OUTB: {
            asm volatile("outb %b0, %w1":: "a"('T'), "Nd"(0xe9) : "memory");
            break;
        }
        case IOCTL_RUN_EMPTY: {
            break;
        }
        default: return -EINVAL;
    }
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = dev_ioctl,
};

static int __init fake_init(void){
    int ret;
    ret = alloc_chrdev_region(&devno, 0, 1, "kvm-fake");
    if (ret) return ret;

    cdev_init(&cdev, &fops);
    ret = cdev_add(&cdev, devno, 1);
    if (ret) goto err_unregister;

    cls = class_create(THIS_MODULE, "kvm-fake");
    if (IS_ERR(cls)) { ret = PTR_ERR(cls); goto err_cdev; }

    if (!device_create(cls, NULL, devno, NULL, "kvm-fake")) {
        ret = -ENOMEM; goto err_class;
    }

    printk(KERN_INFO "kvm-fake module loaded\n");
    return 0;
err_class:
    class_destroy(cls);
err_cdev:
    cdev_del(&cdev);
err_unregister:
    unregister_chrdev_region(devno, 1);
    return ret;
}

static void __exit fake_exit(void){
    device_destroy(cls, devno);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(devno, 1);
    printk(KERN_INFO "kvm-fake module unloaded\n");
}

module_init(fake_init);
module_exit(fake_exit);
MODULE_LICENSE("GPL");