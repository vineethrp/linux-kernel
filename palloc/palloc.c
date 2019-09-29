#include<stdio.h>
#include<stdlib.h>
#include<errno.h>

void *start_mem;
void *end_mem;

#define PAGE_SIZE 4096UL

#define PSTORE_PAGES 1024UL
#define PSTORE_SIZE (PAGE_SIZE * PSTORE_PAGES)

#define NR_CPUS 8
#define PSTORE_PER_CPU_SIZE  (PSTORE_SIZE / NR_CPUS)
#define PSTORE_PER_CPU_PAGES (PSTORE_PAGES / NR_CPUS)

#define PSTORE_IDX_TO_ADDR(cpu, idx) \
	(start_mem + (cpu * PSTORE_PER_CPU_SIZE) + ((idx) * PAGE_SIZE))

#define PSTORE_ADDR_TO_IDX(cpu, addr) \
	(((addr - start_mem) % PSTORE_PER_CPU_SIZE) / PAGE_SIZE);

// XXXX Replace with per-cpu variables
int last_alloc_page_index[NR_CPUS];
int last_freed_page_index[NR_CPUS];

/* 
 * Page allocator design:
 * 
 * The pages are contiguous and we have no free list. To mark page as
 * free, we set the first word to 0xdead and second to 0xbeef.
 *
 * Allocation goes through all pages in circular order and maintains
 * last page that was allocated and freed for faster allocation.
 *
 * This does break if the user writes poison data into the allocated
 * page which incorrectly will result in the page being thought to be free.
 */

static int pstore_mark_page_alloc(void *addr)
{
	int *magic = addr;

	if (magic[0] == 0xdead && magic[1] == 0xbeef) {
		magic[0] = 0;
		magic[1] = 0;
		return 0;
	}

	return 1;
}

// Caller should have done error-checking of addr range
static int pstore_mark_page_free(void *addr)
{
	int *magic = addr;

	if (magic[0] == 0xdead && magic[1] == 0xbeef)
		return -EINVAL;

	// Poison the first 2 ints in the page
	((int *)magic)[0] = 0xdead;
	((int *)magic)[1] = 0xbeef;
	return 0;
}

static int pstore_page_free(void *addr)
{
	// Assume uni processor for now, replace with smp_processor_id()
	int ret, cpu = 0;

	if (addr < PSTORE_IDX_TO_ADDR(cpu, 0) ||
	    addr > PSTORE_IDX_TO_ADDR(cpu, PSTORE_PER_CPU_PAGES - 1))
		return -EINVAL;

	ret = pstore_mark_page_free(addr);
	if (ret)
		return ret;

	// Replace with WRITE_ONCE.
	last_freed_page_index[cpu] = PSTORE_ADDR_TO_IDX(cpu, addr);
	return 0;
}

// Allocate a single free page from the pstore. If errors, return NULL.
static void *pstore_page_alloc(void)
{
	// Assume uni processor for now, replace with smp_processor_id()
	int idx, idx_wrap, scan_nr, cpu = 0;
	int *addr;

	idx = last_freed_page_index[cpu];
	if (idx != -1) {
		last_freed_page_index[cpu] = -1;
		addr = PSTORE_IDX_TO_ADDR(cpu, idx);

		if (pstore_mark_page_alloc(addr) == 0) {
			last_alloc_page_index[cpu] = idx;
			return (void *)addr;
		}
	}

	// Last freed is no longer free, scan from last allocated
	// in the hope that it is likely free.
	idx = last_alloc_page_index[cpu];
	if (idx != -1)
		last_alloc_page_index[cpu] = -1;
	else
		idx = 0;

	scan_nr = 0;

	for (; scan_nr < PSTORE_PER_CPU_PAGES; scan_nr++, idx++) {
		void *addr1;

		idx_wrap = idx % PSTORE_PER_CPU_PAGES;
		addr1 = PSTORE_IDX_TO_ADDR(cpu, idx_wrap);

		if (pstore_mark_page_alloc(addr1) == 0) {
			last_alloc_page_index[cpu] = idx_wrap;
			return addr1;
		}
	}
	
	return NULL;
}

static void init_mem(void)
{
	int i;

	start_mem = malloc(PSTORE_SIZE);
	if (!start_mem)
		printf("ERROR: malloc failed\n");

	for (i = 0; i < PSTORE_PAGES; i++)
		pstore_mark_page_free(start_mem + (i * PAGE_SIZE)); // Replace with PAGE_SHIFT

	end_mem = start_mem + PSTORE_SIZE;

	last_alloc_page_index[0] = -1;
	last_freed_page_index[0] = -1;
	return;
}

int main(int argc, char *argv)
{
	void *a1, *a2, *a3, *a4;
	int i;
	init_mem();

	printf("start_mem = 0x%lx\n", (unsigned long)start_mem);

	for (i = 0; i < 127; i++) {
		a1 = pstore_page_alloc();
		printf("a1 after %d = 0x%lx\n", i, (unsigned long)a1);
	}

	a2 = pstore_page_alloc();
	printf("a2 = 0x%lx\n", (unsigned long)a2);

	printf("a1 free: %d\n", pstore_page_free(a1));

	a4 = pstore_page_alloc();
	printf("a4 = 0x%lx\n", (unsigned long)a4);

	a3 = pstore_page_alloc();
	printf("a3 = 0x%lx\n", (unsigned long)a3);

	a3 = pstore_page_alloc();
	printf("a3 = 0x%lx\n", (unsigned long)a3);

	printf("a4 free: %d\n", pstore_page_free(a4));
	a3 = pstore_page_alloc();
	printf("a3 = 0x%lx\n", (unsigned long)a3);
}

