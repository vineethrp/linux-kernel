/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_BPF_TRACE_H__
#define __LINUX_BPF_TRACE_H__

#include <trace/events/xdp.h>

#define BPF_RAW_TRACEPOINT_OPEN_LAST_FIELD raw_tracepoint.prog_fd

struct bpf_raw_tracepoint {
	struct bpf_raw_event_map *btp;
	struct bpf_prog *prog;
	/*
	 * Multiple programs can be attached to a tracepoint,
	 * All of these are linked to each other and can be reached
	 * from the event's bpf_attach file in tracefs.
	 */
	struct list_head event_attached;
};

struct bpf_raw_tracepoint *bpf_raw_tracepoint_open(char *tp_name, int prog_fd);
void bpf_raw_tracepoint_close(struct bpf_raw_tracepoint *tp);

#endif /* __LINUX_BPF_TRACE_H__ */
