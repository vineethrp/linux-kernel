// SPDX-License-Identifier: GPL-2.0+

#define pr_fmt(fmt) fmt

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/rcupdate.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/completion.h>
#include <linux/moduleparam.h>
#include <linux/percpu.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/freezer.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/srcu.h>
#include <linux/slab.h>
#include <asm/byteorder.h>
#include <linux/torture.h>
#include <linux/vmalloc.h>

#include "rcu.h"

MODULE_LICENSE("GPL");
#ifdef MODULE
# define KFREEPERF_SHUTDOWN 0
#else
# define KFREEPERF_SHUTDOWN 1
#endif

/* Size of a single object to kfree */
#define KFREE_OBJ_BYTES 8

torture_param(int, nthreads, -1, "Number of RCU reader threads");
torture_param(bool, shutdown, KFREEPERF_SHUTDOWN, "Shutdown at end of performance tests.");
torture_param(int, verbose, 1, "Enable verbose debugging printk()s");
torture_param(int, alloc_num, 8000, "Number of allocations and frees done by a thread");
torture_param(int, alloc_size, 16,   "Size of each allocation");
torture_param(int, loops, 10,   "Size of each allocation");
torture_param(int, disable_rcu, 0,   "Disable kfree_rcu and use kfree");

static struct task_struct **reader_tasks;
static struct task_struct *shutdown_task;
static wait_queue_head_t shutdown_wq;
static int nrealthreads;
static atomic_t n_kfree_perf_thread_started;
static atomic_t n_kfree_perf_thread_ended;

/*
 * If performance tests complete, wait for shutdown to commence.
 */
static void kfree_perf_wait_shutdown(void)
{
	cond_resched_tasks_rcu_qs();
	if (atomic_read(&n_kfree_perf_thread_ended) < nrealthreads)
		return;
	while (!torture_must_stop())
		schedule_timeout_uninterruptible(1);
}

struct kfree_obj
{
	char kfree_obj[KFREE_OBJ_BYTES];
	struct rcu_head rh;
};

static int
kfree_perf_thread(void *arg)
{
	int i, l = 0;
	long me = (long)arg;
	struct kfree_obj **alloc_ptrs;
	u64 start_time, end_time;

	pr_err("kfree_perf_thread task started");
	set_cpus_allowed_ptr(current, cpumask_of(me % nr_cpu_ids));
	set_user_nice(current, MAX_NICE);
	atomic_inc(&n_kfree_perf_thread_started);

	alloc_ptrs = (struct kfree_obj **)kmalloc(sizeof(struct kfree_obj *) * alloc_num,
						  GFP_KERNEL);
	if (!alloc_ptrs) {
		pr_err("alloc_ptrs allocation failure\n");
		return -ENOMEM;
	}

	start_time = ktime_get_mono_fast_ns();
	do {
		for (i = 0; i < alloc_num; i++) {
			alloc_ptrs[i] = kmalloc(sizeof(struct kfree_obj), GFP_KERNEL);
			if (!alloc_ptrs[i]) {
				pr_err("alloc_ptrs allocation failure\n");
				return -ENOMEM;
			}
		}

		for (i = 0; i < alloc_num; i++) {
			if (disable_rcu) {
				kfree(alloc_ptrs[i]);
			} else {
				kfree_rcu(alloc_ptrs[i], rh);
			}
		}

		schedule_timeout_uninterruptible(2);
	} while (!torture_must_stop() && ++l < loops);

	kfree(alloc_ptrs);

	if (atomic_inc_return(&n_kfree_perf_thread_ended) >= nrealthreads) {
		end_time = ktime_get_mono_fast_ns();
		pr_err("Total time taken by all kfree'ers: %llu , loops %d\n",
		       (unsigned long long)(end_time - start_time), loops);
		if (shutdown) {
			smp_mb(); /* Assign before wake. */
			wake_up(&shutdown_wq);
		}
	} else {
		kfree_perf_wait_shutdown();
	}

	torture_kthread_stopping("kfree_perf_thread");
	return 0;
}

static void
kfree_perf_cleanup(void)
{
	int i;

	if (torture_cleanup_begin())
		return;

	if (reader_tasks) {
		for (i = 0; i < nrealthreads; i++)
			torture_stop_kthread(kfree_perf_thread,
					     reader_tasks[i]);
		kfree(reader_tasks);
	}

	torture_cleanup_end();
}

/*
 * shutdown kthread.  Just waits to be awakened, then shuts down system.
 */
static int
kfree_perf_shutdown(void *arg)
{
	do {
		wait_event(shutdown_wq,
			   atomic_read(&n_kfree_perf_thread_ended) >=
			   nrealthreads);
	} while (atomic_read(&n_kfree_perf_thread_ended) < nrealthreads);

	smp_mb(); /* Wake before output. */

	kfree_perf_cleanup();
	kernel_power_off();
	return -EINVAL;
}

/*
 * Return the number if non-negative.  If -1, the number of CPUs.
 * If less than -1, that much less than the number of CPUs, but
 * at least one.
 */
static int compute_real(int n)
{
	int nr;

	if (n >= 0) {
		nr = n;
	} else {
		nr = num_online_cpus() + 1 + n;
		if (nr <= 0)
			nr = 1;
	}
	return nr;
}


static int __init
kfree_perf_init(void)
{
	long i;
	int firsterr = 0;

	if (!torture_init_begin("kfree_perf", verbose))
		return -EBUSY;

	nrealthreads = compute_real(nthreads);
	/* Start up the kthreads. */
	if (shutdown) {
		init_waitqueue_head(&shutdown_wq);
		firsterr = torture_create_kthread(kfree_perf_shutdown, NULL,
						  shutdown_task);
		if (firsterr)
			goto unwind;
		schedule_timeout_uninterruptible(1);
	}

	reader_tasks = kcalloc(nrealthreads, sizeof(reader_tasks[0]),
			       GFP_KERNEL);
	if (reader_tasks == NULL) {
		pr_err("out of memory");
		firsterr = -ENOMEM;
		goto unwind;
	}

	for (i = 0; i < nrealthreads; i++) {
		firsterr = torture_create_kthread(kfree_perf_thread, (void *)i,
						  reader_tasks[i]);
		if (firsterr)
			goto unwind;
	}

	while (atomic_read(&n_kfree_perf_thread_started) < nrealthreads)
		schedule_timeout_uninterruptible(1);

	torture_init_end();
	return 0;

unwind:
	torture_init_end();
	kfree_perf_cleanup();
	return firsterr;
}

module_init(kfree_perf_init);
module_exit(kfree_perf_cleanup);
