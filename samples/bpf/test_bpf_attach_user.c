/* Copyright (c) 2016 Facebook
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 */
#define _GNU_SOURCE
#include <sched.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <asm/unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <linux/bpf.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <bpf/bpf.h>
#include "bpf_load.h"

#define MAX_CNT 1000000

static __u64 time_get_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static void test_task_rename(int cpu)
{
	__u64 start_time;
	char buf[] = "test\n";
	int i, fd;

	fd = open("/proc/self/comm", O_WRONLY|O_TRUNC);
	if (fd < 0) {
		printf("couldn't open /proc\n");
		exit(1);
	}
	start_time = time_get_ns();
	for (i = 0; i < MAX_CNT; i++) {
		if (write(fd, buf, sizeof(buf)) < 0) {
			printf("task rename failed: %s\n", strerror(errno));
			close(fd);
			return;
		}
	}
	printf("task_rename:%d: %lld events per sec\n",
	       cpu, MAX_CNT * 1000000000ll / (time_get_ns() - start_time));
	close(fd);
}

static void test_urandom_read(int cpu)
{
	__u64 start_time;
	char buf[4];
	int i, fd;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) {
		printf("couldn't open /dev/urandom\n");
		exit(1);
	}
	start_time = time_get_ns();
	for (i = 0; i < MAX_CNT; i++) {
		if (read(fd, buf, sizeof(buf)) < 0) {
			printf("failed to read from /dev/urandom: %s\n", strerror(errno));
			close(fd);
			return;
		}
	}
	printf("urandom_read:%d: %lld events per sec\n",
	       cpu, MAX_CNT * 1000000000ll / (time_get_ns() - start_time));
	close(fd);
}

static void loop(int cpu, int flags)
{
	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	if (flags & 1)
		test_task_rename(cpu);
	if (flags & 2)
		test_urandom_read(cpu);
}

static void run_perf_test(int tasks, int flags)
{
	pid_t pid[tasks];
	int i;

	for (i = 0; i < tasks; i++) {
		pid[i] = fork();
		if (pid[i] == 0) {
			loop(i, flags);
			exit(0);
		} else if (pid[i] == -1) {
			printf("couldn't spawn #%d process\n", i);
			exit(1);
		}
	}
	for (i = 0; i < tasks; i++) {
		int status;

		assert(waitpid(pid[i], &status, 0) == pid[i]);
		assert(status == 0);
	}
}

static int bpf_trace_attach_prog(int prog_fd, char *subsys_event)
{
	char buf[256];
	int tfd, ret;

	sprintf(buf, "/sys/kernel/debug/tracing/events/%s/bpf_attach", subsys_event);

	tfd = open(buf, O_RDWR);
	if (tfd < 0)
		return tfd;

	sprintf(buf, "%d", prog_fd);

	ret = write(tfd, buf, 256);
	printf("written ret%d bytes\n", ret);

	return 0;
}

int main(int argc, char **argv)
{
	struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	char filename[256];

	setrlimit(RLIMIT_MEMLOCK, &r);

		snprintf(filename, sizeof(filename),
			 "%s_kern.o", argv[0]);
		if (load_bpf_file(filename)) {
			printf("%s", bpf_log_buf);
			return 1;
		}
		close(event_fd[0]);
		close(event_fd[1]);

		printf("prog fd %d\n", prog_fd[0]);
		printf("prog fd %d\n", prog_fd[1]);

		bpf_trace_attach_prog(prog_fd[0], "task/task_rename");
		// bpf_trace_attach_prog(prog_fd[1], "random/urandom_read");


		printf("w/RAW_TRACEPOINT\n");

		run_perf_test(5, ~0);

		while(1);

	return 0;
}
