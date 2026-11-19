#include <stddef.h>
#include <stdlib.h>
#include "../../guest_thread.h"
#include "../../tos_errors.h"
#include "host.h"
#include "tasks.h"

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(ptos_clone_args) == 32 && offsetof(ptos_clone_args, child_pid) == 28,
	"pTOS Pclone v1 layout");
#endif

enum { STACK_BYTES = 64 * 1024, STACK_ALIGNMENT = 16 };

typedef struct thread_start {
	void *(*entry)(void *);
	void *argument;
} thread_start;

static void start(void *argument)
{
	const thread_start *args = argument;
	args->entry(args->argument);
}

int32_t guest_thread_start(guest_thread *thread, void *(*entry)(void *), void *argument)
{
	/* Never publish a thread on a kernel that cannot join it. TID zero
	 * is reserved, so the kernel can check without consuming a result. */
	int32_t result = ptos_call(GEMDOS_OP_PTHJOIN, 0, 0, 0, 0);
	if (result != TOS_ESRCH) return result < 0 ? result : PTOS_ENOSYS;
	thread->stack = malloc(STACK_BYTES + STACK_ALIGNMENT + sizeof(thread_start));
	if (!thread->stack) return PTOS_ENSMEM;
	thread_start *args = thread->stack;
	*args = (thread_start){entry, argument};
	uintptr_t stack = ((uintptr_t)(args + 1) + STACK_ALIGNMENT - 1) & ~(uintptr_t)(STACK_ALIGNMENT - 1);
	ptos_clone_args clone = {
		.flags = PTOS_CLONE_VM | PTOS_CLONE_SAME_PROCESS,
		.entry = start, .argument = args,
		.stack_base = (void *)stack, .stack_size = STACK_BYTES
	};
	result = ptos_call(GEMDOS_OP_PCLONE, (intptr_t)&clone, sizeof(clone), 0, 0);
	if (result < 0)
	{
		free(thread->stack);
		thread->stack = NULL;
		return result;
	}
	thread->tid = result;
	return 0;
}

int32_t guest_thread_join(guest_thread *thread)
{
	/* The stack remains pinned until Pthjoin succeeds. The caller must
	 * free it under the same lock that protects the libcmini allocator. */
	return ptos_call(GEMDOS_OP_PTHJOIN, thread->tid, 0, 0, 0);
}

void guest_thread_yield(void)
{
	ptos_call(GEMDOS_OP_SYIELD, 0, 0, 0, 0);
}
