#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <linux/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (1ULL << PAGE_SHIFT)
#define PAGE_MASK (PAGE_SIZE - 1)

#define BITMAP_CHUNK_FRAMES 6
#define BITMAP_CHUNK_BITS (1 << BITMAP_CHUNK_FRAMES)
#define BITMAP_CHUNK_MASK (BITMAP_CHUNK_BITS - 1)

/* Used for checking that alignment of addr is a bitmap chunk */
#define PAGE_BITMAP_SHIFT (PAGE_SHIFT + BITMAP_CHUNK_FRAMES)
#define PAGE_BITMAP_SIZE  (1ULL << PAGE_BITMAP_SHIFT)
#define PAGE_BITMAP_MASK  (PAGE_BITMAP_SIZE - 1)
#define PAGE_BITMAP_ALIGN(addr)	\
      (void *)(((unsigned long)addr + PAGE_BITMAP_SIZE) & ~PAGE_BITMAP_MASK)

#define TEST_MAP_SIZE (1024 * 1024 * 420)

long get_tns(struct timespec tp2, struct timespec tp)
{
	unsigned long t1, t2;
	
	t1 = (tp.tv_sec * 1000000000ULL) + tp.tv_nsec;
	t2 = (tp2.tv_sec * 1000000000ULL) + tp2.tv_nsec;

	return (t2 - t1);
}

char *prepare_map(void)
{
	char *addr;
	int i;

	addr = mmap(NULL, TEST_MAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

	if (addr == MAP_FAILED)
		addr = NULL;

	// mlock(addr, TEST_MAP_SIZE);

	return addr;
}

/* Check if the marked range is idle */
int check_range_idle(char *addr, unsigned long count)
{
   int fd, ret, pid;
   unsigned long start_pfn, num_pfns, seek_bit, pfn, seek_off, off;
   char buf[128];
   struct timespec tp, tp2;
   uint64_t chunk;
   clockid_t clk_id = CLOCK_MONOTONIC;

   if ((unsigned long)addr & PAGE_MASK)
      return -EINVAL;

   pid = getpid();
   sprintf(buf, "/proc/%d/page_idle", pid);

   fd = open(buf, O_RDONLY);
   if (fd == -1)
      return -errno;

   start_pfn = (unsigned long)addr >> PAGE_SHIFT;
   num_pfns = count / (1ULL << PAGE_SHIFT);

   clock_gettime(clk_id, &tp);

   for (int i = 0; i < num_pfns; i++) {
      pfn = start_pfn + i;
      seek_bit = pfn % BITMAP_CHUNK_BITS;

      printf("reading page idx %d\n", i);

      /* Re-read if we hit boundary, or it is first frame */
      if (seek_bit == 0 || i == 0) {
	 seek_off = (pfn & ~BITMAP_CHUNK_MASK) / 8;
      
	 printf("reading full chunk at page idx %d\n", i);

	 off = lseek(fd, seek_off, SEEK_SET);
	 if (off == -1)
	    return -errno;
	 if (off != seek_off)
	    return -EIO;

	 ret = read(fd, &chunk, sizeof(chunk));
	 if (ret == -1)
	    return -errno;
	 if (ret != sizeof(chunk))
	    return -EIO;

      }

      if ((chunk & (1ULL << seek_bit)) == 0) {
	 printf("chunk sees non zero bit at this position\n");
	 close(fd);
	 return 0;
      }
   }
   clock_gettime(clk_id, &tp2);
   printf("cri time: %ld nanoseconds.\n", get_tns(tp2, tp));
   close(fd);
   return 1;
}

#define WRITE_CHUNK()				\
	 seek_off = (pfn & ~BITMAP_CHUNK_MASK) / 8;\
	 off = lseek(fd, seek_off, SEEK_SET);	\
	 if (off == -1)				\
	    return -errno;			\
	 if (off != seek_off)			\
	    return -EIO;			\
						\
	 ret = write(fd, &chunk, sizeof(chunk));\
	 if (ret == -1)				\
	    return -errno;			\
	 if (ret != sizeof(chunk))		\
	    return -EIO;			\
	 chunk = 0;

/*
 * @addr: starting page-aligned address
 * @count: number of bytes
 */
int mark_range_idle(char *addr, unsigned long count)
{
   int fd, ret;
   unsigned long start_pfn, num_pfns;
   int pid;
   char buf[128];
   struct timespec tp, tp2;
   clockid_t clk_id = CLOCK_MONOTONIC;
   unsigned long seek_off, seek_bit, pfn, off;
   uint64_t chunk = 0;

   if ((unsigned long)addr & PAGE_MASK)
      return -EINVAL;

   pid = getpid();
   sprintf(buf, "/proc/%d/page_idle", pid);

   fd = open(buf, O_WRONLY);
   if (fd == -1)
      return -errno;

   start_pfn = (unsigned long)addr >> PAGE_SHIFT;
   num_pfns = count / (1ULL << PAGE_SHIFT);

   clock_gettime(clk_id, &tp);
   for (int i = 0; i < num_pfns; i++) {

      printf("marking page idx %d\n", i);

      pfn = start_pfn + i;
      seek_bit = pfn % BITMAP_CHUNK_BITS;

      chunk |= (1ULL << seek_bit);

      if ((seek_bit + 1) == BITMAP_CHUNK_BITS) {
	 printf("writing full chunk at page idx %d\n", i);
	 WRITE_CHUNK();
      }
   }

   if (chunk) {
	 printf("writing full chunk at end\n");
	 WRITE_CHUNK();
   }

   clock_gettime(clk_id, &tp2);
   printf("mri time: %ld nanoseconds.\n", get_tns(tp2, tp));
   close(fd);
   return 0;
}

// TODO: fix close fd in err paths on other paths
int write_chunk_idle(char *addr, uint64_t chunk)
{
   int fd, ret;
   unsigned long pfn, seek_off, off;
   int pid;
   char buf[128];

   if ((unsigned long)addr & PAGE_BITMAP_MASK)
      return -EINVAL;

   pid = getpid();
   sprintf(buf, "/proc/%d/page_idle", pid);

   fd = open(buf, O_WRONLY);
   if (fd == -1)
      return -errno;

   pfn = (unsigned long)addr >> PAGE_SHIFT;
   seek_off = pfn / 8;
   off = lseek(fd, seek_off, SEEK_SET);
   if (off == -1) {
      close(fd);
      return -errno;
   }

   if (off != seek_off) {
      close(fd);
      return -EIO;
   }

   ret = write(fd, &chunk, sizeof(chunk));
   if (ret == -1) {
      close(fd);
      return -errno;
   }
   if (ret != sizeof(chunk)) {
      close(fd);
      return -EIO;
   }

   close(fd);
   return 0;
}

int read_chunk_idle(char *addr, uint64_t *chunk)
{
   int fd, ret;
   unsigned long pfn, seek_off, off;
   int pid;
   char buf[128];

   if ((unsigned long)addr & PAGE_BITMAP_MASK)
      return -EINVAL;

   pid = getpid();
   sprintf(buf, "/proc/%d/page_idle", pid);

   fd = open(buf, O_RDONLY);
   if (fd == -1)
      return -errno;

   pfn = (unsigned long)addr >> PAGE_SHIFT;
   seek_off = pfn / 8;
   off = lseek(fd, seek_off, SEEK_SET);
   if (off == -1) {
      close(fd);
      return -errno;
   }

   if (off != seek_off) {
      close(fd);
      return -EIO;
   }

   ret = read(fd, chunk, sizeof(chunk));
   if (ret == -1) {
      close(fd);
      return -errno;
   }
   if (ret != sizeof(chunk)) {
      close(fd);
      return -EIO;
   }

   close(fd);
   return 0;
}

void access_frame(char *b, int f)
{
   /* Access the middle of a frame */
   b[(f << PAGE_SHIFT) + (PAGE_SIZE >> 1)] = 1;
}

   // printf("vfn %lx\n", vfn);		\

#define READ_PM				\
   vfn = (unsigned long)caddr >> 12;	\
   lseek(fd, vfn << 3, SEEK_SET);	\
   read(fd, &pm, sizeof(pm));		\
   printf("page map bytes: %lx\n", pm);

int main()
{
   char *addr = prepare_map();
   unsigned long pm = 0, vfn;
   uint64_t chunk;
   char fpath[256];
   char *caddr;
   caddr = PAGE_BITMAP_ALIGN(addr);
   sprintf(fpath, "/proc/%d/%s", getpid(), "pagemap");
   printf("got addr %lx\n", addr);
   int fd = open(fpath, O_RDONLY);
   if (fd <= 0) {
      return 0;
   }

   printf("The aligned addr is : %lx\n", caddr);
   /* This is the only way to induce memory pressure to swap out idle pages */
   printf("Setting full memset while marking idle as well along the way\n");
   for (char *a = caddr; a < caddr + (TEST_MAP_SIZE - (128 * 4096)); a += (64 * 4096))
   {
      memset(a, 0xfe, (64 * 4096));
      write_chunk_idle(a, 0xffffffffffffffff);
   }

   printf("Read back idle bit: %d\n", read_chunk_idle(caddr, &chunk));
   printf("chunk read back: %llx\n", chunk);
   printf("Read pagemap to confirm page is swapped\n");
   READ_PM;
   return 0;
}
