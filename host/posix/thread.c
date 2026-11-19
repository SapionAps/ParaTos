#include <sched.h>
#include "../../guest_thread.h"
#include "../../tos_errors.h"

int32_t guest_thread_start(guest_thread *thread, void *(*entry)(void *), void *argument)
{
	return pthread_create(thread, NULL, entry, argument) ? TOS_ENSMEM : 0;
}

int32_t guest_thread_join(guest_thread *thread)
{
	return pthread_join(*thread, NULL) ? TOS_EINVAL : 0;
}

void guest_thread_yield(void)
{
	sched_yield();
}
