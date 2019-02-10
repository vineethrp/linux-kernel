#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/prctl.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#define PR_SET_VMA             0x53564d41
#define PR_SET_VMA_ANON_NAME          0

#define MAP_SIZE (300* 1024)

#define DEBUG 1

#ifdef DEBUG
#define dprintf(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
#define dprintf(fmt, args...)
#endif

int read_dmesg(char *str)
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("/proc/kmsg", "r");
    if (fp == NULL)
        return -1;

    while ((read = getline(&line, &len, fp)) != -1) {
        if (strstr(line, str))
            continue;

        printf("%s", line);
    }

    fclose(fp);
    if (line)
        free(line);

    return 0;
}

void print_maps(void)
{
    FILE *fptr;
    char c, filename[] = "/proc/self/maps";

    fptr = fopen(filename, "r");
    if (fptr == NULL) {
        fprintf(stderr, "Cannot open file \n");
        exit(1);
    }

    c = fgetc(fptr);
    while (c != EOF) {
        printf ("%c", c);
        c = fgetc(fptr);
    }

    fclose(fptr);
    return;

}

void print_smaps(void)
{
    FILE *fptr;
    char c, filename[] = "/proc/self/smaps";

    fptr = fopen(filename, "r");
    if (fptr == NULL) {
        fprintf(stderr, "Cannot open file \n");
        exit(1);
    }

    c = fgetc(fptr);
    while (c != EOF) {
        printf ("%c", c);
        c = fgetc(fptr);
    }

    fclose(fptr);
    return;

}

/* Try to determine where in the addr space is there
 * a large chunk of memory that can be MAP_FIXED mmaped
 */
unsigned long get_start_address(void)
{
    void *a;

    a = malloc(1024 * 1024 * 1024); // 1GB
    dprintf("allocated arena: %lx\n", (unsigned long)a);
    free(a);

    // Align start address to 4k boundary, or mmaps on it fail
    return ((unsigned long)a & ~4095);
}

unsigned long last_end;

void *create_map(void)
{
    unsigned long addr;
    int map_flags = MAP_ANONYMOUS | MAP_PRIVATE;

    // Separate start of a map from new map by a page
    if (!last_end)
        addr = get_start_address();
    else
        addr = last_end + (4096);

    if (addr)
        dprintf("last end was 0x%lx, new start 0x%lx\n",  last_end, addr);

    void *m = mmap((void *)addr, MAP_SIZE, PROT_READ | PROT_WRITE, map_flags, -1, 0);

    if (!m || m == MAP_FAILED) {
        fprintf(stderr, "map creation failed, errno: %d\n", errno);
    }

    last_end = (unsigned long)m + MAP_SIZE;

    dprintf("Created map: 0x%lx, updated last_end to 0x%lx\n", (unsigned long)m, last_end);
    return m;
}

int main() {
    char *shm, *shm1, *shm2, *shm3;
    int ret;

    shm   = create_map();
    shm1  = create_map();
    shm2  = create_map();
    shm3  = create_map();

    print_maps();

    ret = prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, shm, MAP_SIZE, (unsigned long)"test-name");
    printf("prctl ret: %d\n", ret);

    read_dmesg("a");
    while (1);

    return 0;
}
