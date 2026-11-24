// SPDX-License-Identifier: GPL-2.0
/*
 * errand_id -- registers a guest process with the VMM so the vmcall exit can pick that
 * process's capability table by its CR3 (w11_vmcall_identity).
 *
 * Phase 0: the device exists and loads; it issues no vmcall. The REGISTER and KERNEL_VMCALL
 * ioctls arrive in step 1.5 with errand_id_abi.h, which is written once in the Composite tree
 * and copied here, never retyped.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

static long
errand_id_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static const struct file_operations errand_id_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = errand_id_ioctl,
	.llseek         = noop_llseek,
};

static struct miscdevice errand_id_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "errand_id",
	.fops  = &errand_id_fops,
	.mode  = 0666,
};

static int __init
errand_id_init(void)
{
	int ret = misc_register(&errand_id_dev);

	if (ret) {
		pr_err("errand_id: misc_register failed (%d)\n", ret);
		return ret;
	}
	pr_info("errand_id: /dev/errand_id ready (phase 0: no operations)\n");
	return 0;
}

static void __exit
errand_id_exit(void)
{
	misc_deregister(&errand_id_dev);
}

module_init(errand_id_init);
module_exit(errand_id_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("w11: per-process identity registration for the vmcall exit");
