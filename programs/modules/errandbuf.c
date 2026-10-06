// SPDX-License-Identifier: GPL-2.0
/*
 * errandbuf -- maps the TLS key service's shared buffer into a guest process.
 *
 * At load it learns the buffer's guest-physical address and length from the service itself
 * (the INFO_GPA / INFO_LEN errands, slot 1), unless both are given as module parameters,
 * and registers /dev/errandbuf. mmap maps the range with the default vm_page_prot, i.e. WB:
 * this guest runs with PAT disabled ("x86/PAT: MTRRs disabled, skipping PAT initialization
 * too", because the VMM reports no MTRRs), so the PAT code honours a WB request for a
 * non-RAM range instead of forcing UC-. `cache=uc` (or wc) maps it uncached instead, as a
 * control for the bandwidth gate.
 *
 * The guest kernel is involved here once, at set-up, and never on the errand path.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <asm/fpu/api.h>

#include "libErrand.h"
#include "tlskey_abi.h"

static unsigned long gpa;
static unsigned long len;
static char *cache = "wb";
module_param(gpa, ulong, 0444);
MODULE_PARM_DESC(gpa, "guest-physical address of the buffer (0: ask the service)");
module_param(len, ulong, 0444);
MODULE_PARM_DESC(len, "length of the buffer in bytes (0: ask the service)");
module_param(cache, charp, 0644);
MODULE_PARM_DESC(cache, "mapping cacheability for the next mmap: wb (default), wc, uc");

static int
errandbuf_mmap(struct file *f, struct vm_area_struct *vma)
{
	unsigned long sz  = vma->vm_end - vma->vm_start;
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;

	if (off >= len || sz > len - off) return -EINVAL;
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
	if (!strcmp(cache, "uc")) vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	else if (!strcmp(cache, "wc")) vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	/* "wb": leave vm_page_prot as is */

	return remap_pfn_range(vma, vma->vm_start, (gpa + off) >> PAGE_SHIFT, sz, vma->vm_page_prot);
}

static const struct file_operations errandbuf_fops = {
	.owner  = THIS_MODULE,
	.mmap   = errandbuf_mmap,
	.llseek = noop_llseek,
};

static struct miscdevice errandbuf_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "errandbuf",
	.fops  = &errandbuf_fops,
	.mode  = 0666,
};

static int __init
errandbuf_init(void)
{
	int ret;

	if (!gpa || !len) {
		long g, l, fault = 0;

		/* The service scrubs XMM0-15; keep the current task's user FPU state out of the way. */
		kernel_fpu_begin();
		g = (long)errand_call_f(ERRAND_CAP(TLSKEY_SLOT), TLSKEY_OP_INFO_GPA, 0, 0, 0, &fault);
		l = (long)errand_call_f(ERRAND_CAP(TLSKEY_SLOT), TLSKEY_OP_INFO_LEN, 0, 0, 0, &fault);
		kernel_fpu_end();
		if (fault || g <= 0 || l <= 0) {
			pr_err("errandbuf: INFO errands failed (fault %ld, gpa %ld, len %ld)\n", fault, g, l);
			return -ENODEV;
		}
		gpa = g;
		len = l;
	}
	if (gpa & ~PAGE_MASK || len & ~PAGE_MASK) {
		pr_err("errandbuf: unaligned buffer gpa %#lx len %#lx\n", gpa, len);
		return -EINVAL;
	}
	ret = misc_register(&errandbuf_dev);
	if (ret) return ret;
	pr_info("errandbuf: /dev/errandbuf -> guest-physical %#lx, %lu KB, cache=%s\n", gpa, len / 1024, cache);

	return 0;
}

static void __exit
errandbuf_exit(void)
{
	misc_deregister(&errandbuf_dev);
}

module_init(errandbuf_init);
module_exit(errandbuf_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("maps the Composite tlskey errand buffer into a guest process");
