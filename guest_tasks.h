#pragma once
#include "guest_thread.h"

typedef struct guest_task {
	struct guest_task *next, *parent;
	guest_thread thread;
	uint32_t basepage;
	unsigned dependents;
	int16_t pid, parent_pid;
	uint16_t status;
	int asynchronous, done, retained, joined;
} guest_task;

typedef struct guest_tasks {
	guest_task *root, *children;
	void (*free_owned)(uint32_t basepage);
} guest_tasks;

/* The backend's execution lock protects the registry and image ownership.
 * A child record is the first member of its heap-allocated backend task.
 * Backends release their native resources before finishing/reaping a task. */
void guest_tasks_init(guest_tasks *tasks, guest_task *root, uint32_t bp, int16_t pid,
	void (*free_owned)(uint32_t));
int32_t guest_task_start(guest_tasks *tasks, guest_task *parent, guest_task *child,
	uint32_t bp, int16_t pid, int asynchronous, void *(*entry)(void *));
void guest_task_finish(guest_tasks *tasks, guest_task *task, uint16_t status);
void guest_task_reap(guest_tasks *tasks, guest_task *task);
/* POSIX also reserves an exited task's basepage until the guest collects
 * its status. Either backend reserves images still used by descendants. */
int guest_task_image_busy(const guest_tasks *tasks, uint32_t bp, int include_unjoined);
int guest_task_have_peer(const guest_tasks *tasks, const guest_task *current);
guest_task *guest_task_next_join(const guest_tasks *tasks);
guest_task *guest_task_wait_child(const guest_tasks *tasks, const guest_task *parent, int16_t pid);
