#pragma once
#include <stdint.h>

#ifdef PARATOS_PTOS
typedef struct guest_thread {
	void *stack;
	int32_t tid;
} guest_thread;
#else
#include <pthread.h>
typedef pthread_t guest_thread;
#endif

/* Returns a GEMDOS result. The caller retains its thread record through join. */
int32_t guest_thread_start(guest_thread *thread, void *(*entry)(void *), void *argument);
int32_t guest_thread_join(guest_thread *thread);
void guest_thread_yield(void);
