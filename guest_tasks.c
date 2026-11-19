#include <stdlib.h>
#include "guest_tasks.h"

void guest_tasks_init(guest_tasks *tasks, guest_task *root, uint32_t bp, int16_t pid,
	void (*free_owned)(uint32_t))
{
	*root = (guest_task){ .basepage = bp, .pid = pid, .retained = 1 };
	*tasks = (guest_tasks){ .root = root, .free_owned = free_owned };
}

int32_t guest_task_start(guest_tasks *tasks, guest_task *parent, guest_task *child,
	uint32_t bp, int16_t pid, int asynchronous, void *(*entry)(void *))
{
	child->parent = parent;
	child->parent_pid = parent->pid;
	child->basepage = bp;
	child->pid = pid;
	child->asynchronous = asynchronous;
	int32_t result = guest_thread_start(&child->thread, entry, child);
	if (result < 0) return result;

	/* The worker takes the execution lock before using the record. */
	child->retained = 1;
	parent->dependents++;
	child->next = tasks->children;
	tasks->children = child;
	return 0;
}

static void sweep(guest_tasks *tasks)
{
	for (guest_task **at = &tasks->children; *at; )
	{
		guest_task *task = *at;
		if (!task->joined || task->retained) { at = &task->next; continue; }
		*at = task->next;
		free(task);
	}
}

void guest_task_finish(guest_tasks *tasks, guest_task *task, uint16_t status)
{
	task->status = status;
	task->done = 1;

	/* Keep the parent image while any descendant can still use p_parent.
	 * Completed children remain waitable for as long as their parent lives. */
	for (guest_task *child = tasks->children; child; child = child->next)
		if (child->parent == task && !child->retained) child->parent = NULL;
	while (task && task->done && task->retained && !task->dependents)
	{
		guest_task *parent = task->parent;
		task->retained = 0;
		tasks->free_owned(task->basepage);
		if (parent)
		{
			parent->dependents--;
			if (parent->done) task->parent = NULL;
		}
		task = parent;
	}
	sweep(tasks);
}

void guest_task_reap(guest_tasks *tasks, guest_task *task)
{
	task->joined = 1;
	sweep(tasks);
}

int guest_task_image_busy(const guest_tasks *tasks, uint32_t bp, int include_unjoined)
{
	if (tasks->root && tasks->root->retained && tasks->root->basepage == bp) return 1;
	for (const guest_task *task = tasks->children; task; task = task->next)
		if (task->basepage == bp && (task->retained || (include_unjoined && !task->joined)))
			return 1;
	return 0;
}

int guest_task_have_peer(const guest_tasks *tasks, const guest_task *current)
{
	if (tasks->root && tasks->root != current && !tasks->root->done) return 1;
	for (const guest_task *task = tasks->children; task; task = task->next)
		if (task != current && !task->done) return 1;
	return 0;
}

guest_task *guest_task_next_join(const guest_tasks *tasks)
{
	for (guest_task *task = tasks->children; task; task = task->next)
		if (!task->joined) return task;
	return NULL;
}

guest_task *guest_task_orphan_ready(const guest_tasks *tasks)
{
	for (guest_task *task = tasks->children; task; task = task->next)
		if (task->done && !task->joined && !task->joining && (!task->parent || task->parent->done))
			return task;
	return NULL;
}

guest_task *guest_task_wait_child(const guest_tasks *tasks, const guest_task *parent, int16_t pid)
{
	guest_task *pending = NULL;
	for (guest_task *task = tasks->children; task; task = task->next)
	{
		if (task->joined || task->joining || !task->asynchronous || task->parent != parent ||
			(pid != -1 && task->pid != pid)) continue;
		if (task->done) return task;
		if (!pending) pending = task;
	}
	return pending;
}
