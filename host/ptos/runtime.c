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
#include "../../tos_errors.h"

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(ptos_clone_args) == 32 && offsetof(ptos_clone_args, child_pid) == 28,
	"pTOS Pclone v1 layout");
#endif

/* The clone shares Musashi and the allocator. The kernel owns each PROCESS's
 * PID, descriptors and directories. Switch the shared emulator only outside
 * m68k_execute(). */
enum {
	SUPER_STACK_BYTES = 4096,
	SUPER_SLOTS = PTOS_SUPER_STACK_BYTES / SUPER_STACK_BYTES,
	NATIVE_STACK_BYTES = 64 * 1024,
	NATIVE_STACK_PREFIX = 64 /* aligned space for the initial native DTA */
};
_Static_assert(sizeof(ptos_native_dta) <= NATIVE_STACK_PREFIX, "native DTA prefix");

typedef struct task {
	struct task *next, *parent;
	void *cpu;
	void *io_buffer;
	ptos_clone_streams *streams;
	ptos_directories dirs;
	ptos_search_state search;
	ptos_super_state super;
	jmp_buf fault_return;
	uint32_t bp, env;
	int32_t stack;
	unsigned super_slot, dependents;
	uint16_t status;
	int done, retained, synchronous, fault_active, native_io, native_exit;
} task;

/* These records retain shared emulator resources. The kernel tracks and
 * publishes PID relationships and collects exit status through Pwaitpid. */
static task *active, *root, *children;
static unsigned cpu_lock;
static int in_slice;
static void (*pending_trap)(void);

static void lock_cpu(void)
{
	while (__atomic_exchange_n(&cpu_lock, 1, __ATOMIC_ACQUIRE))
		ptos_call(GEMDOS_OP_SYIELD, 0, 0, 0, 0);
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

static void select_task(task *t)
{
	active = t;
	ptos_process = t->bp;
	ptos_use_super_state(&t->super, PTOS_ALLOC_START - t->super_slot * SUPER_STACK_BYTES);
	ptos_filesystem_select(t == root ? NULL : &t->search);
	ptos_finished = t->done;
	ptos_status = t->status;
}

static void suspend(void)
{
	m68k_get_context(active->cpu);
	active = NULL;
	unlock_cpu();
}

static void resume(task *t)
{
	lock_cpu();
	select_task(t);
	m68k_set_context(t->cpu);
}

static int have_peer(void)
{
	if (root != active && !root->done) return 1;
	for (task *t = children; t; t = t->next)
		if (t != active && !t->done) return 1;
	return 0;
}

void ptos_task_yield(void)
{
	task *t = active;
	int switch_cpu = t && have_peer() && !ptos_in_overlay();
	if (switch_cpu) suspend();
	ptos_call(GEMDOS_OP_SYIELD, 0, 0, 0, 0);
	if (switch_cpu) resume(t);
}

static task *begin_io(void *buffer)
{
	task *t = active;
	if (!t || ptos_in_overlay()) return NULL;
	/* Some native console calls can Pterm on Ctrl-C. Let the other
	 * guests acquire the CPU lock even if this call never returns. */
	t->native_io = 1;
	t->io_buffer = buffer;
	suspend();
	return t;
}

static void end_io(task *t)
{
	if (!t) return;
	resume(t);
	t->native_io = 0;
	t->io_buffer = NULL;
}

int32_t ptos_task_io(uint16_t fn, intptr_t a, intptr_t b, intptr_t c, void *buffer)
{
	task *t = begin_io(buffer);
	int32_t result = ptos_call(fn, a, b, c, 0);
	end_io(t);
	return result;
}

int32_t ptos_task_bios_io(uint16_t fn, uint16_t device, uint16_t value)
{
	task *t = begin_io(NULL);
	int32_t result = ptos_bios_call(fn, device, value);
	end_io(t);
	return result;
}

int ptos_task_active_image(uint32_t bp)
{
	if (root && root->retained && root->bp == bp) return 1;
	for (task *t = children; t; t = t->next)
		if (t->retained && t->bp == bp) return 1;
	return 0;
}

int ptos_task_active_environment(uint32_t address)
{
	if (root && root->retained && root->env == address) return 1;
	for (task *t = children; t; t = t->next)
		if (t->retained && t->env == address) return 1;
	return 0;
}

static void finish(task *t)
{
	ptos_drop_process(t->bp);
	t->done = 1;

	/* Keep each ancestor's image while a descendant can read p_parent. */
	while (t && t->done && t->retained && !t->dependents)
	{
		task *parent = t->parent;
		t->retained = 0;
		t->parent = NULL;
		ptos_free_owned(t->bp);
		if (parent) parent->dependents--;
		t = parent;
	}
}

static void reap_finished(void)
{
	for (task **at = &children; *at; )
	{
		task *t = *at;
		if (t->stack && (t->done || t->native_io))
		{
			/* A live startup stack is held by the kernel, even after the
			 * worker unlocks. This also works for kernel-reaped orphans
			 * whose PROCESS status is unavailable to this caller. */
			int32_t r = ptos_call(GEMDOS_OP_MFREE, t->stack, 0, 0, 0);
			if (r == 0)
			{
				t->stack = 0;
				if (!t->done)
				{
					/* The kernel has released the startup stack, so the
					 * suspended PROCESS cannot resume. Its native resources
					 * have already been closed in that PROCESS. */
					t->native_exit = 1;
					free(t->io_buffer);
					t->io_buffer = NULL;
					ptos_vdi_forget(t->bp);
					finish(t);
				}
			}
			else if (r != -1) ptos_fault(t->bp, "cannot release native guest stack");
		}
		if (t->stack || t->retained || t->synchronous)
		{
			at = &t->next;
			continue;
		}
		*at = t->next;
		ptos_filesystem_retire(&t->search);
		free(t->cpu);
		free(t);
	}
}

static void run(task *t)
{
	t->fault_active = 1;
	if (setjmp(t->fault_return))
	{
		in_slice = 0;
		pending_trap = NULL;
		t->status = 255;
	}
	else
	{
		if (t != root)
		{
			/* Descriptor-only Pclone starts without a DTA. Keep an idle
			 * native DTA available for restoring after guest search calls. */
			int32_t dta = ptos_call(GEMDOS_OP_FSETDTA, t->stack, 0, 0, 0);
			int32_t dirs = ptos_restore_directories(&t->dirs);
			int32_t files = ptos_clone_streams_install(t->streams);
			free(t->streams);
			t->streams = NULL;
			if (dta < 0 || dirs < 0 || files < 0)
				ptos_fault(t->bp, "cannot initialize native guest resources");
		}
		m68k_pulse_reset();
		for (int i = M68K_REG_D0; i <= M68K_REG_A7; i++)
			m68k_set_reg(i, 0);
		ptos_start_cpu(t->bp);
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
			reap_finished();
			if (!ptos_finished && have_peer()) ptos_task_yield();
		}
		t->status = ptos_status;
	}
	t->fault_active = 0;
}

static void process_main(void *argument)
{
	task *t = argument;
	lock_cpu();
	select_task(t);
	run(t);
	finish(t);
	uint16_t status = t->status;
	active = NULL;
	unlock_cpu();
	ptos_call(GEMDOS_OP_PTERM, status, 0, 0, 0);
	__builtin_trap(); /* Pterm cannot return */
}

static int32_t wait_native(int16_t pid, int16_t flags, uint32_t *usage)
{
	task *t = active;
	int blocking = t && !(flags & 1) && !ptos_in_overlay();
	if (blocking) suspend();
	int32_t r = ptos_call(GEMDOS_OP_PWAITPID, pid, flags, (intptr_t)usage, 0);
	if (blocking) resume(t);
	return r == TOS_EFILNF ? TOS_ECHILD : r;
}

int32_t ptos_task_go(uint32_t bp, int take_ownership, int asynchronous)
{
	if (!active || ptos_in_overlay())
		return asynchronous ? PTOS_ENOSYS : ptos_go(bp, take_ownership);
	if (ptos_task_active_image(bp)) return TOS_EACCDN;
	if (!ptos_image_startable(bp, take_ownership)) return PTOS_EIMBA;

	unsigned slots = 1;
	for (task *child = children; child; child = child->next)
		if (!child->done) slots |= 1u << child->super_slot;
	unsigned slot = 1;
	while (slot < SUPER_SLOTS && (slots & (1u << slot))) slot++;
	if (slot == SUPER_SLOTS) return PTOS_ENSMEM;

	task *t = calloc(1, sizeof(*t));
	if (!t) return PTOS_ENSMEM;
	t->bp = bp;
	t->cpu = malloc(m68k_context_size());
	int32_t result = t->cpu ? ptos_task_directories(bp, &t->dirs) : PTOS_ENSMEM;
	if (result >= 0) result = ptos_clone_streams_prepare(bp, &t->streams);
	if (result >= 0)
	{
		result = ptos_call(GEMDOS_OP_MALLOC, NATIVE_STACK_PREFIX + NATIVE_STACK_BYTES, 0, 0, 0);
		if (result > 0) t->stack = result;
		else if (!result) result = PTOS_ENSMEM;
	}
	int32_t pid = 0;
	if (result >= 0)
	{
		ptos_clone_args clone = {
			.flags = PTOS_CLONE_VM | PTOS_CLONE_COPY_ALL_HANDLES | PTOS_CLONE_COPY_CWD,
			.entry = process_main, .argument = t,
			.stack_base = (void *)(uintptr_t)(t->stack + NATIVE_STACK_PREFIX),
			.stack_size = NATIVE_STACK_BYTES, .child_pid = &pid
		};
		result = ptos_call(GEMDOS_OP_PCLONE, (intptr_t)&clone, sizeof(clone), 0, 0);
	}
	if (result < 0)
	{
		if (t->stack) ptos_call(GEMDOS_OP_MFREE, t->stack, 0, 0, 0);
		free(t->streams);
		free(t->cpu);
		free(t);
		/* Retain synchronous in-place GO for kernels without Pclone. */
		if (result == PTOS_ENOSYS && !asynchronous && active == root && !children)
			return ptos_go(bp, take_ownership);
		return result;
	}
	if (!result || pid <= 0 || pid > INT16_MAX)
		ptos_fault(bp, "Pclone did not return a valid PROCESS PID");

	/* process_main must acquire the lock before reading the published task. */
	t->env = m68k_read_memory_32(bp + BP_ENV);
	t->super_slot = slot;
	t->synchronous = !asynchronous;
	t->retained = 1;
	t->parent = active;
	active->dependents++;
	t->next = children;
	children = t;
	ptos_drop_process(bp);
	if (take_ownership) ptos_take_image(bp);
	m68k_write_memory_32(bp + BP_PARENT, ptos_process);
	if (asynchronous) return pid;

	result = wait_native(pid, 0, NULL);
	/* The native public wait packs an eight-bit code and a signal byte.
	 * Synchronous GEMDOS Pexec returns the child's full Pterm word. */
	if (result >= 0) result = t->done && !t->native_exit ? t->status : (uint16_t)result;
	t->synchronous = 0;
	reap_finished();
	return result;
}

int32_t ptos_task_wait(int16_t pid, int16_t flags, uint32_t usage)
{
	uint32_t native_usage[2] = {0};
	if (usage) ptos_guest_buffer(usage, sizeof(native_usage));
	if ((flags & ~3) || pid == 0 || pid < -1) return PTOS_EINVAL;
	int32_t result = wait_native(pid, flags, usage ? native_usage : NULL);
	if (usage && result > 0)
		for (unsigned i = 0; i < 2; i++) m68k_write_memory_32(usage + 4 * i, native_usage[i]);
	return result;
}

int32_t ptos_task_exec(uint16_t mode, const char *path, const char *tail, const char *env)
{
	uint16_t operation = mode & 0x7fff;
	if (active && operation == PTOS_EXEC_REPLACE && (active != root || children))
		return PTOS_ENOSYS;
	task *t = active;
	int blocking = t && operation == PTOS_EXEC_LOAD_GO && !ptos_in_overlay();
	if (blocking) suspend();
	int32_t r = ptos_call(GEMDOS_OP_PEXEC, mode, (intptr_t)path, (intptr_t)tail, (intptr_t)env);
	if (blocking) resume(t);
	return r;
}

int32_t ptos_run_program(uint32_t bp)
{
	ptos_process_cleanup();
	static task initial;
	initial = (task){ .bp = bp, .retained = 1 };
	initial.cpu = malloc(m68k_context_size());
	if (!initial.cpu) return PTOS_ENSMEM;

	lock_cpu();
	root = &initial;
	select_task(root);
	root->env = m68k_read_memory_32(bp + BP_ENV);
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	ptos_take_image(bp);
	run(root);
	finish(root);

	/* Keep the shared emulator alive until its last guest leaves. Kernel
	 * PROCESS lifetime and orphan reaping do not depend on this list. */
	while (children)
	{
		reap_finished();
		if (children) ptos_task_yield();
	}
	free(root->cpu);
	active = root = NULL;
	ptos_use_super_state(NULL, PTOS_ALLOC_START);
	ptos_process_cleanup();
	unlock_cpu();
	return initial.status;
}
