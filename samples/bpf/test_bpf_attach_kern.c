// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018 Facebook */
#include <uapi/linux/bpf.h>
#include "bpf_helpers.h"

SEC("raw_tracepoint/task_rename")
int prog(struct bpf_raw_tracepoint_args *ctx)
{
	char fmt[] = "rename\n";

	bpf_trace_printk(fmt, 8);
	return 0;
}

SEC("raw_tracepoint/urandom_read")
int prog2(struct bpf_raw_tracepoint_args *ctx)
{
	char fmt[] = "uread\n";

	bpf_trace_printk(fmt, 7);
	return 0;
}
char _license[] SEC("license") = "GPL";
