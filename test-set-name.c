/*
 * Example memfd_create(2) server application.
 *
 * Copyright (C) 2015 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 * SPDX-License-Identifier: Unlicense
 *
 * Kindly check attached README.md file for further details.
 */

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <linux/un.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#define PR_SET_VMA             0x53564d41
#define PR_SET_VMA_ANON_NAME          0

int main() {
    char *shm;
    int ret;

    shm = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

    ret = prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, shm, 8192, (unsigned long)"test-name");

    printf("prctl ret: %d\n", ret);

    while (1);

    return 0;
}
