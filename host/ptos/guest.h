#pragma once
#include <stdint.h>

/* Offsets of the actual 256-byte 68k basepage, not a native C structure. */
enum { BP_LOW = 0, BP_HIGH = 4, BP_TEXT = 8, BP_TLEN = 12,
	BP_DATA = 16, BP_DLEN = 20, BP_BSS = 24, BP_BLEN = 28,
	BP_DTA = 32, BP_PARENT = 36, BP_ENV = 44, BP_TAIL = 128, BP_SIZE = 256 };

extern uint32_t ptos_process;
void ptos_allocator_init(void);
void ptos_allocator_destroy(void);
int32_t ptos_alloc(int32_t size);
int32_t ptos_free(uint32_t address);
int32_t ptos_shrink(uint32_t address, int32_t size);
void ptos_take_image(uint32_t basepage);
int ptos_image_valid(uint32_t basepage);
void ptos_free_owned(uint32_t owner);
int32_t ptos_load(const char *path, const char *tail, const char *environment);
int32_t ptos_basepage(const char *tail, const char *environment);
int32_t ptos_exec(uint16_t mode, uint32_t path, uint32_t tail, uint32_t env);
int32_t ptos_go(uint32_t basepage, int take_ownership);
void ptos_terminate(uint16_t status);
int32_t ptos_super(uint32_t stack);
int32_t ptos_supexec(uint32_t function);
int ptos_super_return(void);
void ptos_start_cpu(uint32_t basepage);
int32_t ptos_run_program(uint32_t basepage);
void ptos_process_cleanup(void);
int32_t ptos_files_init(void);
int32_t ptos_files_push(void);
int32_t ptos_files_pop(void);
void ptos_files_cleanup(void);
int32_t ptos_file_handle(int16_t handle);
int32_t ptos_file_open(uint16_t fn, const char *path, uint16_t mode);
int32_t ptos_file_dup(int16_t handle);
int32_t ptos_file_close(int16_t handle);
int32_t ptos_file_force(int16_t standard, int16_t handle);

extern int ptos_finished;
extern uint16_t ptos_status;
