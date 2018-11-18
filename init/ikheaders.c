#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

extern char __ikheaders_start[];
extern unsigned long __ikheaders_size;
static int keep_ikheaders;
static char *ikheaders_bin;
static unsigned long ikheaders_size;

static int __init set_keep_ikheaders(char *str)
{
	keep_ikheaders = 1;
	return 1;
}

__setup("kheaders", set_keep_ikheaders);

static ssize_t
ikheaders_read_current(struct file *file, char __user *buf,
		      size_t len, loff_t * offset)
{

	if (WARN_ON(!ikheaders_size))
		return 0;

	return simple_read_from_buffer(buf, len, offset,
				       ikheaders_bin,
				       ikheaders_size);
}

static const struct file_operations ikheaders_file_ops = {
	.read = ikheaders_read_current,
	.llseek = default_llseek,
};

static int __init ikheaders_init(void)
{
	struct proc_dir_entry *entry;

	if (!keep_ikheaders)
		return 0;

	ikheaders_bin = vmalloc(__ikheaders_size);
	if (WARN_ON(!ikheaders_bin))
		return 0;

	memcpy(ikheaders_bin, __ikheaders_start, __ikheaders_size);
	ikheaders_size = __ikheaders_size;

	entry = proc_create("kheaders.tbz", S_IFREG | S_IRUGO, NULL,
			    &ikheaders_file_ops);
	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, __ikheaders_size);

	return 0;

}

late_initcall(ikheaders_init);
