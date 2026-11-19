#pragma once
#include <stdint.h>

/* Native pTOS Pclone v1, introduced by pTOS PR #903. Pointers use the
 * caller's ILP32 ABI, including x32. TLS is reserved and must be NULL. */
enum { PTOS_CLONE_VM = 1, PTOS_CLONE_SAME_PROCESS = 2 };
typedef struct ptos_clone_args {
	uint32_t flags;
	void (*entry)(void *);
	void *argument;
	void *stack_base;
	uint32_t stack_size;
	void *tls;
	int32_t *child_tid;
	int32_t *child_pid;
} ptos_clone_args;

typedef struct ptos_directories {
	int32_t drive;
	uint32_t directories;
	char cwd[32][129];
} ptos_directories;

typedef struct ptos_super_state {
	struct { uint32_t pc, sr, sp; } frames[16];
	unsigned int depth, floor;
} ptos_super_state;

typedef struct ptos_files_state ptos_files_state;
int32_t ptos_task_files_prepare(uint32_t bp, ptos_files_state **out);
void ptos_task_files_commit(uint32_t bp, ptos_files_state *files);
int32_t ptos_task_files_suspend(void);
void ptos_task_files_cancel_suspend(void);
int32_t ptos_task_files_resume(ptos_files_state *files);
void ptos_task_files_destroy(ptos_files_state *files);
void ptos_task_files_release_root(void);

int32_t ptos_save_directories(ptos_directories *dirs);
int32_t ptos_restore_directories(const ptos_directories *dirs);
int32_t ptos_task_directories(uint32_t bp, ptos_directories *dirs);
void ptos_use_super_state(ptos_super_state *state, uint32_t stack);
int ptos_in_overlay(void);

int ptos_defer_trap(void (*dispatch)(void));
int32_t ptos_task_go(uint32_t bp, int take_ownership, int asynchronous);
int ptos_task_active_image(uint32_t bp);
int ptos_task_active_environment(uint32_t address);
int32_t ptos_task_pid(int parent);
int32_t ptos_task_wait(int16_t pid, int16_t flags, uint32_t usage);
int32_t ptos_task_exec(uint16_t mode, const char *path, const char *tail, const char *env);
void ptos_task_yield(void);
