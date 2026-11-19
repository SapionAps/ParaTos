#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"
#include "tasks.h"
#include "filesystem.h"
#include "vdi.h"
#include "../../guest_exec.h"
#include "../../guest_tasks.h"
#include "../../tos_errors.h"

/* Pclone shares the address space and native files. There is no native TLS
 * on these targets. Bind the CPU, libcmini and native process state to one
 * guest at a time, and switch only outside m68k_execute(). */
enum { SUPER_STACK_BYTES = 4096, SUPER_SLOTS = PTOS_SUPER_STACK_BYTES / SUPER_STACK_BYTES };

typedef struct task {
	guest_task guest;
	void *cpu;
	ptos_files_state *files;
	ptos_directories dirs;
	ptos_super_state super;
	jmp_buf fault_return;
	uint32_t env;
	unsigned super_slot;
	int started, fault_active;
} task;

typedef struct native_child {
	struct native_child *next;
	task *owner;
	int16_t pid, native_pid;
} native_child;

static task *active, *root;
static guest_tasks guests;
static native_child *native_children;
static unsigned cpu_lock;
static int in_slice;
static void (*pending_trap)(void);
static int32_t join(task *t, int prepared);

static void lock_cpu(void)
{
	while (__atomic_exchange_n(&cpu_lock, 1, __ATOMIC_ACQUIRE))
		guest_thread_yield();
}

static void unlock_cpu(void)
{
	__atomic_store_n(&cpu_lock, 0, __ATOMIC_RELEASE);
}

_Noreturn void ptos_fault(uint32_t address, const char *reason)
{
	fprintf(stderr, "Paratos: %s at guest address %08lx\n", reason, (unsigned long)address);
	if (active && active->fault_active)
	{
		fprintf(stderr, "guest PC=%08lx SP=%08lx SR=%04lx\n",
			(unsigned long)m68k_get_reg(NULL, M68K_REG_PC),
			(unsigned long)m68k_get_reg(NULL, M68K_REG_SP),
			(unsigned long)m68k_get_reg(NULL, M68K_REG_SR));
		longjmp(active->fault_return, 1);
	}
	exit(1);
	__builtin_unreachable();
}

int ptos_defer_trap(void (*dispatch)(void))
{
	if (!in_slice) return 0;
	pending_trap = dispatch;
	m68k_end_timeslice();
	return 1;
}

static int32_t prepare_suspend(void)
{
	int32_t result = ptos_save_directories(&active->dirs);
	if (result < 0) return result;
	result = ptos_task_files_suspend();
	if (result < 0) return result;
	m68k_get_context(active->cpu);
	return 0;
}

static void suspend(void)
{
	active = NULL;
	unlock_cpu();
}

static int32_t activate(task *t)
{
	active = t;
	ptos_process = t->guest.basepage;
	ptos_use_super_state(&t->super, PTOS_ALLOC_START - t->super_slot * SUPER_STACK_BYTES);
	if (t->started) m68k_set_context(t->cpu);
	int32_t dirs = ptos_restore_directories(&t->dirs);
	int32_t files = ptos_task_files_resume(t->files);
	ptos_finished = t->guest.done;
	ptos_status = t->guest.status;
	return dirs < 0 ? dirs : files;
}

static int32_t resume(task *t)
{
	lock_cpu();
	return activate(t);
}

void ptos_task_yield(void)
{
	task *t = active;
	if (t && guest_task_have_peer(&guests, &active->guest) && !ptos_in_overlay())
	{
		if (prepare_suspend() < 0) ptos_fault(ptos_process, "cannot save guest directories or streams");
		suspend();
		guest_thread_yield();
		if (resume(t) < 0) ptos_fault(t->guest.basepage, "cannot restore guest directories or streams");
	}
	else
		guest_thread_yield();
}

static int pid_used(int16_t pid)
{
	if (root->guest.pid == pid) return 1;
	for (guest_task *child = guests.children; child; child = child->next)
		if (!child->joined && child->pid == pid) return 1;
	for (native_child *c = native_children; c; c = c->next)
		if (c->pid == pid) return 1;
	return 0;
}

static int16_t allocate_pid(void)
{
	for (int16_t pid = INT16_MAX; pid > 0; pid--)
		if (!pid_used(pid)) return pid;
	return 0;
}

int32_t ptos_task_pid(int parent)
{
	if (active && active != root) return parent ? active->guest.parent_pid : active->guest.pid;
	return ptos_call(parent ? GEMDOS_OP_PGETPPID : GEMDOS_OP_PGETPID, 0, 0, 0, 0);
}

int ptos_task_active_image(uint32_t bp)
{
	return guest_task_image_busy(&guests, bp, 0);
}

int ptos_task_active_environment(uint32_t address)
{
	if (root && root->guest.retained && root->env == address) return 1;
	for (guest_task *child = guests.children; child; child = child->next)
		if (child->retained && ((task *)child)->env == address) return 1;
	return 0;
}

static void finish(task *t)
{
	ptos_vdi_drop(t->guest.basepage);
	ptos_filesystem_drop(t->guest.basepage);
	if (t == root) ptos_task_files_release_root();
	else ptos_task_files_destroy(t->files);
	t->files = NULL;
	for (native_child *c = native_children; c; c = c->next)
		if (c->owner == t) c->owner = NULL;
	guest_task_finish(&guests, &t->guest, t->guest.status);
}

static void reap_native_orphans(void)
{
	for (native_child **at = &native_children; *at; )
	{
		native_child *c = *at;
		int32_t result = c->owner ? 0 : ptos_call(GEMDOS_OP_PWAITPID, c->native_pid, 1, 0, 0);
		if (result <= 0 && result != TOS_EFILNF)
		{
			at = &c->next;
			continue;
		}
		*at = c->next;
		free(c);
	}
}

/* A completed guest has dropped the execution lock and is exiting its
 * native thread. Join outside Musashi and leave a live parent's waits alone. */
static void reap_guest_orphans(void)
{
	guest_task *orphan;
	while ((orphan = guest_task_orphan_ready(&guests)))
		if (join((task *)orphan, 0) < 0) break;
}

static void run(task *t)
{
	t->fault_active = 1;
	if (setjmp(t->fault_return))
	{
		in_slice = 0;
		pending_trap = NULL;
		t->guest.status = 255;
	}
	else
	{
		if (t != root && activate(t) < 0)
			ptos_fault(t->guest.basepage, "cannot start guest with inherited directories or streams");
		m68k_pulse_reset();
		for (int i = M68K_REG_D0; i <= M68K_REG_A7; i++)
			m68k_set_reg(i, 0);
		ptos_start_cpu(t->guest.basepage);
		t->started = 1;
		while (!ptos_finished)
		{
			in_slice = 1;
			m68k_execute(10000);
			in_slice = 0;
			if (pending_trap)
			{
				void (*dispatch)(void) = pending_trap;
				pending_trap = NULL;
				dispatch();
			}
			reap_native_orphans();
			if (!ptos_finished)
			{
				reap_guest_orphans();
				if (guest_task_have_peer(&guests, &active->guest)) ptos_task_yield();
			}
		}
		t->guest.status = ptos_status;
	}
	t->fault_active = 0;
}

static void *thread_main(void *argument)
{
	task *t = argument;
	lock_cpu();
	run(t);
	finish(t);
	active = NULL;
	unlock_cpu();
	return NULL;
}

static int32_t join(task *t, int prepared)
{
	task *caller = active;
	int32_t result = prepared ? 0 : prepare_suspend();
	if (result < 0) return result;
	t->guest.joining = 1;
	suspend();
	result = guest_thread_join(&t->guest.thread);
	int32_t restored = resume(caller);
	t->guest.joining = 0;
	if (result >= 0)
	{
		free(t->guest.thread.stack);
		free(t->cpu);
		t->cpu = NULL;
		result = t->guest.done ? t->guest.status : 255;
		guest_task_reap(&guests, &t->guest);
	}
	if (restored < 0) ptos_fault(caller->guest.basepage, "cannot restore guest after Pthjoin");
	return result;
}

int32_t ptos_task_go(uint32_t bp, int take_ownership, int asynchronous)
{
	if (!active || ptos_in_overlay())
		return asynchronous ? PTOS_ENOSYS : ptos_go(bp, take_ownership);
	if (ptos_task_active_image(bp)) return TOS_EACCDN;
	if (!ptos_image_startable(bp, take_ownership)) return PTOS_EIMBA;

	unsigned slots = 1;
	for (guest_task *child = guests.children; child; child = child->next)
		if (!child->done) slots |= 1u << ((task *)child)->super_slot;
	unsigned slot = 1;
	while (slot < SUPER_SLOTS && (slots & (1u << slot))) slot++;
	int16_t pid = allocate_pid();
	if (slot == SUPER_SLOTS || !pid) return PTOS_ENSMEM;

	task *t = calloc(1, sizeof(*t));
	if (!t) return PTOS_ENSMEM;
	t->cpu = malloc(m68k_context_size());
	int32_t result = t->cpu ? ptos_task_directories(bp, &t->dirs) : PTOS_ENSMEM;
	if (result >= 0) result = ptos_task_files_prepare(bp, &t->files);
	int prepared = 0;
	if (result >= 0 && !asynchronous)
	{
		result = prepare_suspend();
		prepared = result >= 0;
	}
	if (result >= 0)
		result = guest_task_start(&guests, &active->guest, &t->guest,
			bp, pid, asynchronous, thread_main);
	if (result < 0)
	{
		if (prepared) ptos_task_files_cancel_suspend();
		ptos_task_files_destroy(t->files);
		free(t->cpu);
		free(t);
		/* Older kernels keep the synchronous in-place path. */
		if (result == PTOS_ENOSYS && !asynchronous && active == root && !guests.children)
			return ptos_go(bp, take_ownership);
		return result;
	}
	t->env = m68k_read_memory_32(bp + BP_ENV);
	t->super_slot = slot;
	ptos_task_files_commit(bp, t->files);
	ptos_drop_process(bp);
	if (take_ownership) ptos_take_image(bp);
	m68k_write_memory_32(bp + BP_PARENT, ptos_process);
	if (asynchronous) return pid;
	result = join(t, prepared);
	if (result < 0) t->guest.asynchronous = 1;
	return result;
}

int32_t ptos_task_wait(int16_t pid, int16_t flags, uint32_t usage)
{
	uint32_t native_usage[2] = {0};
	if (usage) ptos_guest_buffer(usage, sizeof(native_usage));
	if ((flags & ~3) || pid == 0 || pid < -1) return PTOS_EINVAL;
	if (!active) return PTOS_ENOSYS;

	int32_t result;
	for (;;)
	{
		guest_task *child = guest_task_wait_child(&guests, &active->guest, pid);
		int pending = child != NULL;
		if (child && (child->done || (!(flags & 1) && pid != -1)))
		{
			int16_t child_pid = child->pid;
			result = join((task *)child, 0);
			if (result < 0) return result;
			result = guest_wait_status(child_pid, result & 255);
			goto done;
		}
		for (native_child **at = &native_children; *at; at = &(*at)->next)
		{
			native_child *c = *at;
			if (c->owner != active || (pid != -1 && pid != c->pid)) continue;
			int32_t r = ptos_call(GEMDOS_OP_PWAITPID, c->native_pid, flags | 1,
				usage ? (intptr_t)native_usage : 0, 0);
			if (!r) { pending = 1; continue; }
			if (r < 0 && r != TOS_EFILNF) return r;
			result = r < 0 ? TOS_ECHILD : guest_wait_status(c->pid, (uint16_t)r);
			*at = c->next;
			free(c);
			goto done;
		}
		if (!pending) return TOS_ECHILD;
		if (flags & 1) return 0;
		ptos_task_yield();
	}
done:
	if (usage && result >= 0)
		for (unsigned i = 0; i < 2; i++) m68k_write_memory_32(usage + 4 * i, native_usage[i]);
	return result;
}

int32_t ptos_task_exec(uint16_t mode, const char *path, const char *tail, const char *env)
{
	uint16_t operation = mode & 0x7fff;
	if (active && operation == PTOS_EXEC_REPLACE && (active != root || guests.children))
		return PTOS_ENOSYS;
	if (!active || operation != PTOS_EXEC_LOAD_GO_ASYNC)
		return ptos_call(GEMDOS_OP_PEXEC, mode, (intptr_t)path, (intptr_t)tail, (intptr_t)env);

	int16_t spare = allocate_pid();
	if (!spare) return PTOS_ENSMEM;
	native_child *child = malloc(sizeof(*child));
	if (!child) return PTOS_ENSMEM;
	int32_t result = ptos_call(GEMDOS_OP_PEXEC, mode, (intptr_t)path, (intptr_t)tail, (intptr_t)env);
	if (result <= 0) { free(child); return result; }
	child->owner = active;
	child->native_pid = result;
	child->pid = pid_used(result) ? spare : result;
	child->next = native_children;
	native_children = child;
	return child->pid;
}

int32_t ptos_run_program(uint32_t bp)
{
	ptos_process_cleanup();
	int32_t files = ptos_files_init();
	if (files < 0) { ptos_process_cleanup(); return files; }
	static task initial;
	initial = (task){0};
	initial.cpu = malloc(m68k_context_size());
	if (!initial.cpu) { ptos_process_cleanup(); return PTOS_ENSMEM; }

	lock_cpu();
	active = root = &initial;
	guest_tasks_init(&guests, &root->guest, bp, ptos_task_pid(0), ptos_free_owned);
	root->env = m68k_read_memory_32(bp + BP_ENV);
	ptos_finished = 0;
	ptos_status = 0;
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	ptos_process = bp;
	ptos_take_image(bp);
	ptos_use_super_state(&root->super, PTOS_ALLOC_START);
	run(root);
	finish(root);

	/* Live guests may be joining their own children. Reap only completed
	 * orphans while they run, then collect any remaining shutdown records. */
	while (guest_task_have_peer(&guests, &root->guest))
	{
		reap_guest_orphans();
		ptos_task_yield();
	}
	guest_task *child;
	while ((child = guest_task_next_join(&guests)))
		if (join((task *)child, 0) < 0)
			ptos_fault(bp, "cannot join guest at emulator shutdown");
	reap_native_orphans();
	while (native_children)
	{
		native_child *next = native_children->next;
		free(native_children);
		native_children = next;
	}
	free(root->cpu);
	active = root = NULL;
	guests.root = NULL;
	ptos_use_super_state(NULL, PTOS_ALLOC_START);
	ptos_process_cleanup();
	unlock_cpu();
	return initial.guest.status;
}
