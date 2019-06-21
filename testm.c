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

#define TEST_MAP_SIZE (1024 * 1024 * 12)

char *prepare_map(void)
{
	char *addr;
	int i;

	addr = mmap(NULL, TEST_MAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

	if (addr == MAP_FAILED)
		addr = NULL;

	memset(addr, 0xfe, TEST_MAP_SIZE);
	mlock(addr, TEST_MAP_SIZE);

	return addr;
}

/* Check if the marked range is idle */
int check_range_idle(char *addr, unsigned long count)
{
   int fd, ret;
   unsigned long start_pfn, num_pfns;
   int pid;
   char buf[128];

   if ((unsigned long)addr & PAGE_MASK)
      return -EINVAL;

   pid = getpid();
   sprintf(buf, "/proc/%d/page_idle", pid);

   fd = open(buf, O_RDONLY);
   if (fd == -1)
      return -errno;

   start_pfn = (unsigned long)addr >> PAGE_SHIFT;
   num_pfns = count / (1ULL << PAGE_SHIFT);

   for (int i = 0; i < num_pfns; i++) {
      unsigned long seek_off, seek_bit, pfn, off;
      uint64_t chunk;

      pfn = start_pfn + i;
      seek_off = (pfn & ~BITMAP_CHUNK_MASK) / 8;
      seek_bit = pfn % BITMAP_CHUNK_BITS;

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

      if ((chunk & (1ULL << seek_bit)) == 0) {
	 close(fd);
	 return 0;
      }
   }

   close(fd);
   return 1;
}

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

   if ((unsigned long)addr & PAGE_MASK)
      return -EINVAL;

   pid = getpid();
   sprintf(buf, "/proc/%d/page_idle", pid);

   fd = open(buf, O_WRONLY);
   if (fd == -1)
      return -errno;

   start_pfn = (unsigned long)addr >> PAGE_SHIFT;
   num_pfns = count / (1ULL << PAGE_SHIFT);

   for (int i = 0; i < num_pfns; i++) {
      unsigned long seek_off, seek_bit, pfn, off;
      uint64_t chunk;

      pfn = start_pfn + i;
      seek_off = (pfn & ~BITMAP_CHUNK_MASK) / 8;
      seek_bit = pfn % BITMAP_CHUNK_BITS;
      chunk = (1ULL << seek_bit);

      off = lseek(fd, seek_off, SEEK_SET);
      if (off == -1)
	 return -errno;
      if (off != seek_off)
	 return -EIO;

      ret = write(fd, &chunk, sizeof(chunk));
      if (ret == -1)
	 return -errno;
      if (ret != sizeof(chunk))
	 return -EIO;
   }

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

int main()
{
   char *addr = prepare_map();
   char *caddr;
   uint64_t chunk;

   printf("pid %d %p\n", getpid(), addr);

   // SINGLE CHUNK TESTS

   caddr = PAGE_BITMAP_ALIGN(addr + (TEST_MAP_SIZE/2));
   printf("wci: %d\n", write_chunk_idle(caddr, 0xfefefefefefefefe));

   printf("Expecting: %llx\n",
	 0xfefefefefefefefe &  ~(((1ULL << 6)|(1ULL << 42))));

   access_frame(caddr, 6);
   access_frame(caddr, 42);

   printf("rci: %d\n", read_chunk_idle(caddr, &chunk));
   printf("chunk read back: %llx\n", chunk);

   printf("No access test\n");
   printf("mri %d\n", mark_range_idle(addr, TEST_MAP_SIZE / 2));
   printf("is_idle: %d\n", check_range_idle(addr, TEST_MAP_SIZE / 2));

   printf("Access test\n");
   printf("mri %d\n", mark_range_idle(addr, TEST_MAP_SIZE / 2));
   addr[2] = 1;
   printf("is_idle: %d\n", check_range_idle(addr, TEST_MAP_SIZE / 2));

   printf("No access test\n");
   printf("mri %d\n", mark_range_idle(addr, TEST_MAP_SIZE / 2));
   printf("is_idle: %d\n", check_range_idle(addr, TEST_MAP_SIZE / 2));

   printf("access test\n");
   printf("mri %d\n", mark_range_idle(addr, TEST_MAP_SIZE / 2));
   addr[2] = 1;
   printf("is_idle: %d\n", check_range_idle(addr, TEST_MAP_SIZE / 2));
 
   return 0;
}
