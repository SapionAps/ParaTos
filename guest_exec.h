#pragma once
#include <stdint.h>

enum guest_go_mode {
	GUEST_GO = 4, GUEST_GO_FREE = 6, GUEST_GO_ASYNC = 104, GUEST_GO_FREE_ASYNC = 106
};

static inline int guest_go_mode(uint16_t mode, int *release, int *asynchronous)
{
	*release = mode == GUEST_GO_FREE || mode == GUEST_GO_FREE_ASYNC;
	*asynchronous = mode == GUEST_GO_ASYNC || mode == GUEST_GO_FREE_ASYNC;
	return mode == GUEST_GO || *release || *asynchronous;
}

static inline int32_t guest_wait_status(int16_t pid, uint16_t status)
{
	return (int32_t)(((uint32_t)(uint16_t)pid << 16) | status);
}
