// SPDX-License-Identifier: GPL-2.0
/*
 * kernel/kheaders.c
 * Provide headers and artifacts needed to build kernel modules.
 * (Borrowed code from kernel/configs.c)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/uaccess.h>

/*
 * Define kernel_headers_data and kernel_headers_data_size, which contains the
 * compressed kernel headers.  The file is first compressed with xz and then
 * bounded by two eight byte magic numbers to allow extraction from a binary
 * kernel image:
 *
 *   IKHD_ST
 *   <image>
 *   IKHD_ED
 */
#define KH_MAGIC_START	"IKHD_ST"
#define KH_MAGIC_END	"IKHD_ED"
#include "kheaders_data.h"


#define KH_MAGIC_SIZE (sizeof(KH_MAGIC_START) - 1)
#define kernel_headers_data_size \
	(sizeof(kernel_headers_data) - 1 - KH_MAGIC_SIZE * 2)

static ssize_t
ikheaders_read_current(struct file *file, char __user *buf,
		      size_t len, loff_t *offset)
{
	return simple_read_from_buffer(buf, len, offset,
				       kernel_headers_data + KH_MAGIC_SIZE,
				       kernel_headers_data_size);
}

static const struct file_operations ikheaders_file_ops = {
	.owner = THIS_MODULE,
	.read = ikheaders_read_current,
	.llseek = default_llseek,
};

static int __init ikheaders_init(void)
{
	struct proc_dir_entry *entry;

	/* create the current headers file */
	entry = proc_create("kheaders.txz", S_IFREG | S_IRUGO, NULL,
			    &ikheaders_file_ops);
	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, kernel_headers_data_size);

	return 0;
}

static void __exit ikheaders_cleanup(void)
{
	remove_proc_entry("kheaders.txz", NULL);
}

module_init(ikheaders_init);
module_exit(ikheaders_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joel Fernandes");
MODULE_DESCRIPTION("Echo the kernel header artifacts used to build the kernel");
