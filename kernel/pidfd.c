// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/kernel/pidfd.c
 *
 *  pidfd related operations
 *
 *  2019-03-23: Added pidfd_wait system call
 *              (Joel Fernandes and Daniel Colascione - Google)
 */

#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/pid_namespace.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/syscalls.h>
#include <linux/uio.h>

/*
 * Set the task's exit_state to state only if it is in a ZOMBIE state.
 * Return true if the exit_state could be set, otherwise false.
 */
bool task_set_exit_state_if_zombie(struct task_struct *task, int state)
{
	lockdep_assert_lock_held(&tasklist_lock);

	if (cmpxchg(&task->exit_state, EXIT_ZOMBIE, state) != EXIT_ZOMBIE)
		return false;

	wake_up_all(&task->signal->wait_pidfd);
	return true;
}

/* 
 * Set the task's exit state
 * Also note that do_notify_parent() is always called before
 * this is called.
 */
void task_set_exit_state(struct task_struct *task, int state)
{
	lockdep_assert_lock_held(&tasklist_lock);

	task->exit_state = state;
	wake_up_all(&task->signal->wait_pidfd);
	return;
}

static __poll_t pidfd_wait_poll(struct file *file, struct poll_table_struct *pts)
{
	int poll_flags = 0;

	read_lock(&tasklist_lock);

	task = get_proc_task(file->f_path.dentry->d_inode);
	if (!task || task->exit_state == EXIT_DEAD)
		poll_flags = POLLIN | POLLRDNORM | POLLERR;
	else if (task->exit_state == EXIT_ZOMBIE)
		poll_flags = (POLLIN | POLLRDNORM);

	if (!poll_flags)
		poll_wait(file, &task->signal->wait_pidfd, pts);

	read_unlock(&tasklist_lock);
	return poll_flags;
}

static const struct file_operations pidfd_wait_file_ops = {
	.poll		= pidfd_wait_poll,
};
