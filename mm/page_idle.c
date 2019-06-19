// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/mmzone.h>
#include <linux/page_ext.h>
#include <linux/page_idle.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/sched/mm.h>
#include <linux/swap.h>
#include <linux/swapops.h>

#define BITMAP_CHUNK_SIZE	sizeof(u64)
#define BITMAP_CHUNK_BITS	(BITMAP_CHUNK_SIZE * BITS_PER_BYTE)

/*
 * Get a reference to a page for idle tracking purposes, with additional checks.
 *
 * Idle page tracking only considers user memory pages, for other types of
 * pages the idle flag is always unset and an attempt to set it is silently
 * ignored.
 *
 * We treat a page as a user memory page if it is on an LRU list, because it is
 * always safe to pass such a page to rmap_walk(), which is essential for idle
 * page tracking. With such an indicator of user pages we can skip isolated
 * pages, but since there are not usually many of them, it will hardly affect
 * the overall result.
 */
static struct page *page_idle_get_page(struct page *page_in)
{
	struct page *page;
	pg_data_t *pgdat;

	page = page_in;
	if (!page || !PageLRU(page) ||
	    !get_page_unless_zero(page))
		return NULL;

	pgdat = page_pgdat(page);
	spin_lock_irq(&pgdat->lru_lock);
	if (unlikely(!PageLRU(page))) {
		put_page(page);
		page = NULL;
	}
	spin_unlock_irq(&pgdat->lru_lock);
	return page;
}

/*
 * This function tries to get a user memory page by pfn as described above.
 */
static struct page *page_idle_get_page_pfn(unsigned long pfn)
{

	if (!pfn_valid(pfn))
		return NULL;

	return page_idle_get_page(pfn_to_page(pfn));
}

static bool page_idle_clear_pte_refs_one(struct page *page,
					struct vm_area_struct *vma,
					unsigned long addr, void *arg)
{
	struct page_vma_mapped_walk pvmw = {
		.page = page,
		.vma = vma,
		.address = addr,
	};
	bool referenced = false;

	while (page_vma_mapped_walk(&pvmw)) {
		addr = pvmw.address;
		if (pvmw.pte) {
			/*
			 * For PTE-mapped THP, one sub page is referenced,
			 * the whole THP is referenced.
			 */
			if (ptep_clear_young_notify(vma, addr, pvmw.pte))
				referenced = true;
		} else if (IS_ENABLED(CONFIG_TRANSPARENT_HUGEPAGE)) {
			if (pmdp_clear_young_notify(vma, addr, pvmw.pmd))
				referenced = true;
		} else {
			/* unexpected pmd-mapped page? */
			WARN_ON_ONCE(1);
		}
	}

	if (referenced) {
		clear_page_idle(page);
		/*
		 * We cleared the referenced bit in a mapping to this page. To
		 * avoid interference with page reclaim, mark it young so that
		 * page_referenced() will return > 0.
		 */
		set_page_young(page);
	}
	return true;
}

static void page_idle_clear_pte_refs(struct page *page)
{
	/*
	 * Since rwc.arg is unused, rwc is effectively immutable, so we
	 * can make it static const to save some cycles and stack.
	 */
	static const struct rmap_walk_control rwc = {
		.rmap_one = page_idle_clear_pte_refs_one,
		.anon_lock = page_lock_anon_vma_read,
	};
	bool need_lock;

	if (!page_mapped(page) ||
	    !page_rmapping(page))
		return;

	need_lock = !PageAnon(page) || PageKsm(page);
	if (need_lock && !trylock_page(page))
		return;

	rmap_walk(page, (struct rmap_walk_control *)&rwc);

	if (need_lock)
		unlock_page(page);
}

/* Helper to get the start and end frame given a pos and count */
static int page_idle_get_frames(loff_t pos, size_t count, struct mm_struct *mm,
				unsigned long *start, unsigned long *end)
{
	unsigned long max_frame;

	/* If an mm is not given, assume we want physical frames */
	max_frame = mm ? (mm->task_size >> PAGE_SHIFT) : max_pfn;

	if (pos % BITMAP_CHUNK_SIZE || count % BITMAP_CHUNK_SIZE)
		return -EINVAL;

	*start = pos * BITS_PER_BYTE;
	if (*start >= max_frame)
		return -ENXIO;

	*end = *start + count * BITS_PER_BYTE;
	if (*end > max_frame)
		*end = max_frame;
	return 0;
}

static bool page_idle_pte_check(struct page *page)
{
	if (!page)
		return false;

	if (page_is_idle(page)) {
		/*
		 * The page might have been referenced via a
		 * pte, in which case it is not idle. Clear
		 * refs and recheck.
		 */
		page_idle_clear_pte_refs(page);
		if (page_is_idle(page))
			return true;
	}

	return false;
}

static ssize_t page_idle_bitmap_read(struct file *file, struct kobject *kobj,
				     struct bin_attribute *attr, char *buf,
				     loff_t pos, size_t count)
{
	u64 *out = (u64 *)buf;
	struct page *page;
	unsigned long pfn, end_pfn;
	int bit, ret;

	ret = page_idle_get_frames(pos, count, NULL, &pfn, &end_pfn);
	if (ret == -ENXIO)
		return 0;  /* Reads beyond max_pfn do nothing */
	else if (ret)
		return ret;

	for (; pfn < end_pfn; pfn++) {
		bit = pfn % BITMAP_CHUNK_BITS;
		if (!bit)
			*out = 0ULL;
		page = page_idle_get_page_pfn(pfn);
		if (page && page_idle_pte_check(page)) {
			*out |= 1ULL << bit;
			put_page(page);
		}
		if (bit == BITMAP_CHUNK_BITS - 1)
			out++;
		cond_resched();
	}
	return (char *)out - buf;
}

static ssize_t page_idle_bitmap_write(struct file *file, struct kobject *kobj,
				      struct bin_attribute *attr, char *buf,
				      loff_t pos, size_t count)
{
	const u64 *in = (u64 *)buf;
	struct page *page;
	unsigned long pfn, end_pfn;
	int bit, ret;

	ret = page_idle_get_frames(pos, count, NULL, &pfn, &end_pfn);
	if (ret)
		return ret;

	for (; pfn < end_pfn; pfn++) {
		bit = pfn % BITMAP_CHUNK_BITS;
		if ((*in >> bit) & 1) {
			page = page_idle_get_page_pfn(pfn);
			if (page) {
				page_idle_clear_pte_refs(page);
				set_page_idle(page);
				put_page(page);
			}
		}
		if (bit == BITMAP_CHUNK_BITS - 1)
			in++;
		cond_resched();
	}
	return (char *)in - buf;
}

static struct bin_attribute page_idle_bitmap_attr =
		__BIN_ATTR(bitmap, 0600,
			   page_idle_bitmap_read, page_idle_bitmap_write, 0);

static struct bin_attribute *page_idle_bin_attrs[] = {
	&page_idle_bitmap_attr,
	NULL,
};

static const struct attribute_group page_idle_attr_group = {
	.bin_attrs = page_idle_bin_attrs,
	.name = "page_idle",
};

#ifndef CONFIG_64BIT
static bool need_page_idle(void)
{
	return true;
}
struct page_ext_operations page_idle_ops = {
	.need = need_page_idle,
};
#endif

/*  page_idle tracking for /proc/<pid>/page_idle */

struct page_node {
	struct page *page;
	unsigned long addr;
	struct list_head list;
};

struct page_idle_proc_priv {
	unsigned long start_addr;
	char *buffer;
	int write;

	/* Pre-allocate and provide nodes to pte_page_idle_proc_add() */
	struct page_node *page_nodes;
	int cur_page_node;
	struct list_head *idle_page_list;
};

/*
 * Add page to list to be set as idle later.
 */
static void pte_page_idle_proc_add(struct page *page,
			       unsigned long addr, struct mm_walk *walk)
{
	struct page *page_get = NULL;
	struct page_node *pn;
	int bit;
	unsigned long frames;
	struct page_idle_proc_priv *priv = walk->private;
	u64 *chunk = (u64 *)priv->buffer;

	if (priv->write) {
		VM_BUG_ON(!page);

		/* Find whether this page was asked to be marked */
		frames = (addr - priv->start_addr) >> PAGE_SHIFT;
		bit = frames % BITMAP_CHUNK_BITS;
		chunk = &chunk[frames / BITMAP_CHUNK_BITS];
		if (((*chunk >> bit) & 1) == 0)
			return;
	}

	if (page) {
		page_get = page_idle_get_page(page);
		if (!page_get)
			return;
	}

	/*
	 * For all other pages, add it to a list since we have to walk rmap,
	 * which acquires ptlock, and we cannot walk rmap right now.
	 */
	pn = &(priv->page_nodes[priv->cur_page_node++]);
	pn->page = page_get;
	pn->addr = addr;
	list_add(&pn->list, priv->idle_page_list);
}

static int pte_page_idle_proc_range(pmd_t *pmd, unsigned long addr,
				    unsigned long end,
				    struct mm_walk *walk)
{
	pte_t *pte;
	spinlock_t *ptl;
	struct page *page;
	struct vm_area_struct *vma = walk->vma;

	ptl = pmd_trans_huge_lock(pmd, vma);
	if (ptl) {
		if (pmd_present(*pmd)) {
			page = follow_trans_huge_pmd(vma, addr, pmd,
						     FOLL_DUMP|FOLL_WRITE);
			if (!IS_ERR_OR_NULL(page))
				pte_page_idle_proc_add(page, addr, walk);
		}
		spin_unlock(ptl);
		return 0;
	}

	if (pmd_trans_unstable(pmd))
		return 0;

	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	for (; addr != end; pte++, addr += PAGE_SIZE) {
		if (!pte_present(*pte))
			continue;

		page = vm_normal_page(vma, addr, *pte);
		if (page)
			pte_page_idle_proc_add(page, addr, walk);
	}

	pte_unmap_unlock(pte - 1, ptl);
	return 0;
}

ssize_t page_idle_proc_generic(struct file *file, char __user *ubuff,
			       size_t count, loff_t *pos, int write)
{
	int ret;
	char *buffer;
	u64 *out;
	unsigned long start_addr, end_addr, start_frame, end_frame;
	struct mm_struct *mm = file->private_data;
	struct mm_walk walk = { .pmd_entry = pte_page_idle_proc_range, };
	struct page_node *cur;
	struct page_idle_proc_priv priv;
	bool walk_error = false;
	LIST_HEAD(idle_page_list);

	if (!mm || !mmget_not_zero(mm))
		return -EINVAL;

	if (count > PAGE_SIZE)
		count = PAGE_SIZE;

	buffer = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto out_mmput;
	}
	out = (u64 *)buffer;

	if (write && copy_from_user(buffer, ubuff, count)) {
		ret = -EFAULT;
		goto out;
	}

	ret = page_idle_get_frames(*pos, count, mm, &start_frame, &end_frame);
	if (ret)
		goto out;

	start_addr = (start_frame << PAGE_SHIFT);
	end_addr = (end_frame << PAGE_SHIFT);
	priv.buffer = buffer;
	priv.start_addr = start_addr;
	priv.write = write;

	priv.idle_page_list = &idle_page_list;
	priv.cur_page_node = 0;
	priv.page_nodes = kzalloc(sizeof(struct page_node) *
				  (end_frame - start_frame), GFP_KERNEL);
	if (!priv.page_nodes) {
		ret = -ENOMEM;
		goto out;
	}

	walk.private = &priv;
	walk.mm = mm;

	down_read(&mm->mmap_sem);

	/*
	 * idle_page_list is needed because walk_page_vma() holds ptlock which
	 * deadlocks with page_idle_clear_pte_refs(). So we have to collect all
	 * pages first, and then call page_idle_clear_pte_refs().
	 */
	ret = walk_page_range(start_addr, end_addr, &walk);
	if (ret)
		walk_error = true;

	list_for_each_entry(cur, &idle_page_list, list) {
		int bit, index;
		unsigned long off;
		struct page *page = cur->page;

		if (unlikely(walk_error))
			goto remove_page;

		if (write) {
			if (page) {
				page_idle_clear_pte_refs(page);
				set_page_idle(page);
			}
		} else {
			if (page_idle_pte_check(page)) {
				off = ((cur->addr) >> PAGE_SHIFT) - start_frame;
				bit = off % BITMAP_CHUNK_BITS;
				index = off / BITMAP_CHUNK_BITS;
				out[index] |= 1ULL << bit;
			}
		}
remove_page:
		if (page)
			put_page(page);
	}

	if (!write && !walk_error)
		ret = copy_to_user(ubuff, buffer, count);

	up_read(&mm->mmap_sem);
	kfree(priv.page_nodes);
out:
	kfree(buffer);
out_mmput:
	mmput(mm);
	if (!ret)
		ret = count;
	return ret;

}

ssize_t page_idle_proc_read(struct file *file, char __user *ubuff,
			    size_t count, loff_t *pos)
{
	return page_idle_proc_generic(file, ubuff, count, pos, 0);
}

ssize_t page_idle_proc_write(struct file *file, char __user *ubuff,
			     size_t count, loff_t *pos)
{
	return page_idle_proc_generic(file, ubuff, count, pos, 1);
}

static int __init page_idle_init(void)
{
	int err;

	err = sysfs_create_group(mm_kobj, &page_idle_attr_group);
	if (err) {
		pr_err("page_idle: register sysfs failed\n");
		return err;
	}
	return 0;
}
subsys_initcall(page_idle_init);
