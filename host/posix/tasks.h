#pragma once
#include "common.h"
#include <sys/types.h>
#include <poll.h>

/* CPU and current_process are TLS; this lock protects shared guest memory,
 * allocator lists and child lifecycle records. Blocking waits drop it. */
void PosixPause(void);
void PosixResume(void);
int PosixRunRoot(emuptr32_t basepage);
void PosixTerminate(uint16_t status);
int32_t PosixGo(emuptr32_t basepage, int release, int asynchronous);
int PosixWaitThread(int16_t pid, int16_t flags, emuptr32_t usage, int32_t *result);
int PosixHasThreads(void);
int16_t PosixTaskPid(void);
int16_t PosixTaskParentPid(void);
int16_t PosixRuntimeRootPid(void);
int32_t PosixLoadImage(const char *path, emuptr32_t tail, emuptr32_t env);
int32_t PosixCreateBasepage(emuptr32_t tail, emuptr32_t env);

/* Shared PID namespace for native children and emulated threads. */
int16_t PosixRegisterPid(int host_pid);
void PosixForgetPid(int host_pid);
ssize_t PosixRead(int fd, void *buffer, size_t count);
ssize_t PosixWrite(int fd, const void *buffer, size_t count);
int PosixPoll(struct pollfd *fds, nfds_t count, int timeout);
int PosixHostFd(int fd);
int PosixClose(int fd);
int PosixForce(int target, int source);
int PosixAdoptFd(int fd);
void PosixExportStandardHandles(void);
void PosixTerminalLock(void);
void PosixTerminalUnlock(void);
uint64_t PosixTaskOwner(void);
void PosixRootIdentity(int16_t id, int16_t parent);
int16_t PosixReservePid(int synchronous);
void PosixReleasePid(int16_t id);
void PosixBindPid(int16_t id, int native_pid, int synchronous);
int32_t PosixEnvironmentSize(uint32_t env);
int PosixOrphanChildren(uint64_t owner);
int PosixReapOrphans(void);
