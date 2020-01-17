#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define RCU_READER_DELAY 100 //ms
#define RCU_BLOCKER_DELAY 600 //ms

MODULE_LICENSE("GPL");

struct sched_param {
	int sched_priority;
};

int stop_test = 0;
int test_pass = 1;
int reader_exit = 0;
s64 delta_fail;

#define ns_to_ms(delta) (delta / 1000000ULL)

static int rcu_reader(void *a)
{
	ktime_t start, end, reader_begin;
	s64 delta;

	reader_begin = ktime_get();

	while (!kthread_should_stop() && !stop_test) {
		rcu_read_lock();
		trace_printk("rcu_reader entering RSCS\n");
		start = ktime_get();
		mdelay(RCU_READER_DELAY);
		end = ktime_get();
		trace_printk("rcu_reader exiting RSCS\n");
		rcu_read_lock();
		delta = ktime_to_ns(ktime_sub(end, start));

		if (delta < 0 || (ns_to_ms(delta) > (2 * RCU_READER_DELAY))) {
			delta_fail = delta;
			test_pass = 0;
			break;
		}

		// Don't let the rcu_reader() run more than 3s inorder to
		// not starve the blocker incase reader prio > blocker prio.
		delta = ktime_to_ns(ktime_sub(end, reader_begin));
		if (ns_to_ms(delta) > 3000)
			break;
	}

	stop_test = 1;
	reader_exit = 1;
	pr_err("Exiting reader\n");
	return 0;
}

static int rcu_blocker(void *a)
{
	int loops = 5;

	while (!kthread_should_stop() && loops-- && !stop_test) {
		trace_printk("rcu_blocker entering\n");
		mdelay(RCU_BLOCKER_DELAY);
		trace_printk("rcu_blocker exiting\n");
	}

	pr_err("Exiting blocker\n");
	stop_test = 1;

	// Wait for reader to finish
	while (!reader_exit)
		schedule_timeout_uninterruptible(1);

	if (test_pass)
		pr_err("TEST PASSED\n");
	else
		pr_err("TEST FAILED, failing delta=%lldms\n", ns_to_ms(delta_fail));

	return 0;
}

static int __init ptest_init(void){
	struct sched_param params;
	struct task_struct *reader, *blocker;

	reader = kthread_create(rcu_reader, NULL, "reader");
	params.sched_priority = 50;
	sched_setscheduler(reader, SCHED_FIFO, &params);
	kthread_bind(reader, smp_processor_id());

	blocker = kthread_create(rcu_blocker, NULL, "blocker");
	params.sched_priority = 60;
	sched_setscheduler(blocker, SCHED_FIFO, &params);
	kthread_bind(blocker, smp_processor_id());

	wake_up_process(reader);

	// Let reader run a little
	mdelay(50);

	wake_up_process(blocker);
	return 0;
}

static void __exit ptest_exit(void){
}

module_init(ptest_init);
module_exit(ptest_exit);
