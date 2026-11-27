#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"
#include "filesystem.h"
#include "vdi.h"
#include "tasks.h"
#include "../../guest_exec.h"
#include "../../guest_cpu.h"

int ptos_finished;
uint16_t ptos_status;

typedef struct parent {
	struct parent *next;
	void *cpu;
	uint32_t bp;
	unsigned int super_depth, super_floor;
	ptos_directories directory;
} parent;
static parent *parents;
static unsigned int parent_depth;
typedef struct loaded_context {
	struct loaded_context *next;
	uint32_t bp;
	ptos_directories directory;
} loaded_context;
static loaded_context *loaded;

#define SUPER_RETURN 0x8000u
static ptos_super_state root_super;
static ptos_super_state *super = &root_super;
static uint32_t super_stack = PTOS_ALLOC_START;

void ptos_use_super_state(ptos_super_state *state, uint32_t stack)
{
	super = state ? state : &root_super;
	super_stack = stack;
}

int ptos_in_overlay(void) { return parents != NULL; }

int32_t ptos_save_directories(ptos_directories *p)
{
	p->drive = ptos_call(GEMDOS_OP_DGETDRV, 0, 0, 0, 0);
	if (p->drive < 0) return p->drive;
	p->directories = 0;
	for (unsigned int i = 0; i < 32; i++)
	{
		/* Dgetpath returns a drive-relative path; restore it from root. */
		p->cwd[i][0] = '\\';
		int32_t r = ptos_call(GEMDOS_OP_DGETPATH, (intptr_t)(p->cwd[i] + 1), i + 1, 0, 0);
		if (r == -46 || r == -34) continue; /* absent drive or unmounted medium */
		if (r < 0) return r;
		char *relative = p->cwd[i] + 1;
		while (*relative == '\\' || *relative == '/') relative++;
		memmove(p->cwd[i] + 1, relative, strlen(relative) + 1);
		p->directories |= UINT32_C(1) << i;
	}
	return 0;
}

int32_t ptos_restore_directories(const ptos_directories *p)
{
	int32_t result = 0;
	for (unsigned int i = 0; i < 32; i++)
	{
		if (!(p->directories & (UINT32_C(1) << i))) continue;
		ptos_call(GEMDOS_OP_DSETDRV, i, 0, 0, 0);
		int32_t r = ptos_call(GEMDOS_OP_DSETPATH, (intptr_t)p->cwd[i], 0, 0, 0);
		if (r < 0) result = r;
	}
	ptos_call(GEMDOS_OP_DSETDRV, p->drive, 0, 0, 0);
	return result;
}

int32_t ptos_task_directories(uint32_t bp, ptos_directories *dirs)
{
	for (loaded_context *child = loaded; child; child = child->next)
		if (child->bp == bp) { *dirs = child->directory; return 0; }
	return ptos_save_directories(dirs);
}

int32_t ptos_capture_process(uint32_t bp)
{
	/* The initial native launch already has its inherited host context.
	 * Internal PE_LOAD/PE_BASEPAGE children inherit at creation time. */
	if (!ptos_process) return 0;
	loaded_context *p = calloc(1, sizeof(*p));
	if (!p) return PTOS_ENSMEM;
	int32_t r = ptos_save_directories(&p->directory);
	if (r >= 0) r = ptos_files_capture(bp);
	if (r < 0) { free(p); return r; }
	p->bp = bp; p->next = loaded; loaded = p;
	return 0;
}

void ptos_drop_process(uint32_t bp)
{
	ptos_vdi_drop(bp);
	ptos_files_drop(bp);
	ptos_filesystem_drop(bp);
	for (loaded_context **at = &loaded; *at; at = &(*at)->next)
	{
		loaded_context *p = *at;
		if (p->bp != bp) continue;
		*at = p->next; free(p);
		return;
	}
}

void ptos_process_cleanup(void)
{
	ptos_vdi_cleanup();
	ptos_files_cleanup();
	ptos_filesystem_cleanup();
	while (parents)
	{
		parent *p = parents;
		parents = p->next;
		ptos_restore_directories(&p->directory);
		free(p->cpu); free(p);
	}
	parent_depth = super->depth = super->floor = 0;
}

void ptos_start_cpu(uint32_t bp)
{
	/* A nested guest inherits the unused top of the emulated supervisor
	 * stack. Resetting it would overwrite a parent's active Supexec frame. */
	uint32_t supervisor = parent_depth ? m68k_get_reg(NULL, M68K_REG_ISP) : super_stack;
	guest_cpu_start(bp, m68k_read_memory_32(bp + BP_TEXT),
		m68k_read_memory_32(bp + BP_HIGH), supervisor);
}

int32_t ptos_go(uint32_t bp, int take_ownership)
{
	if (bp == ptos_process || !ptos_image_valid(bp)) return PTOS_EIMBA;
	if (parent_depth == 16) return PTOS_ENSMEM;
	uint32_t text = m68k_read_memory_32(bp + BP_TEXT);
	ptos_guest_buffer(text, 2);
	parent *p = calloc(1, sizeof(*p));
	if (!p) return PTOS_ENSMEM;
	p->cpu = malloc(m68k_context_size());
	if (!p->cpu) { free(p); return PTOS_ENSMEM; }
	int32_t result = ptos_save_directories(&p->directory);
	if (result < 0) { free(p->cpu); free(p); return result; }
	for (loaded_context *child = loaded; child; child = child->next)
		if (child->bp == bp) { result = ptos_restore_directories(&child->directory); break; }
	if (result >= 0) result = ptos_files_push(bp);
	if (result < 0)
	{
		int32_t restored = ptos_restore_directories(&p->directory);
		free(p->cpu); free(p);
		if (restored < 0) ptos_fault(ptos_process, "cannot restore overlay parent directory");
		return result;
	}
	ptos_drop_process(bp);
	m68k_get_context(p->cpu);
	p->bp = ptos_process; p->super_depth = super->depth;
	p->super_floor = super->floor;
	p->next = parents; parents = p; parent_depth++;
	if (take_ownership) ptos_take_image(bp);
	m68k_write_memory_32(bp + BP_PARENT, ptos_process);
	ptos_process = bp;
	super->floor = super->depth;
	ptos_start_cpu(bp);
	return 0;
}

void ptos_terminate(uint16_t status)
{
	if (parents)
	{
		ptos_vdi_drop(ptos_process);
		ptos_filesystem_drop(ptos_process);
		ptos_free_owned(ptos_process);
		parent *p = parents;
		ptos_process = p->bp; super->depth = p->super_depth;
		super->floor = p->super_floor;
		m68k_set_context(p->cpu);
		m68k_set_reg(M68K_REG_D0, status);
		parents = p->next; parent_depth--;
		int32_t restored = ptos_restore_directories(&p->directory);
		int32_t files = ptos_files_pop();
		free(p->cpu); free(p);
		if (restored < 0) ptos_fault(ptos_process, "cannot restore overlay parent directory");
		if (files < 0) ptos_fault(ptos_process, "cannot restore overlay parent streams");
		return;
	}
	ptos_status = status;
	ptos_finished = 1;
	m68k_end_timeslice();
}

int32_t ptos_super(uint32_t stack)
{
	uint32_t sr = m68k_get_reg(NULL, M68K_REG_SR);
	if (stack == 1) return sr & 0x2000 ? -1 : 0;
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint32_t old = m68k_get_reg(NULL, M68K_REG_ISP);
	if (!(sr & 0x2000))
	{
		if (stack) ptos_guest_buffer(stack - 4, 4);
		m68k_set_reg(M68K_REG_SR, sr | 0x2000);
		m68k_set_reg(M68K_REG_SP, stack ? stack : sp);
	}
	else
	{
		m68k_set_reg(M68K_REG_USP, sp);
		m68k_set_reg(M68K_REG_SR, sr & ~0x2000);
		m68k_set_reg(M68K_REG_ISP, stack ? stack : old);
	}
	return old;
}

int32_t ptos_supexec(uint32_t function)
{
	ptos_guest_buffer(function, 2);
	if (super->depth == sizeof(super->frames) / sizeof(super->frames[0])) return PTOS_ENSMEM;
	super->frames[super->depth].pc = m68k_get_reg(NULL, M68K_REG_PC);
	super->frames[super->depth].sr = m68k_get_reg(NULL, M68K_REG_SR);
	super->frames[super->depth].sp = m68k_get_reg(NULL, M68K_REG_SP);
	super->depth++;
	m68k_write_memory_16(SUPER_RETURN, 0x4e4e); /* TRAP #14: private return gate */
	m68k_set_reg(M68K_REG_SR, m68k_get_reg(NULL, M68K_REG_SR) | 0x2000);
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	if (sp < 4) ptos_fault(sp, "invalid guest supervisor stack");
	m68k_write_memory_32(sp - 4, SUPER_RETURN);
	m68k_set_reg(M68K_REG_SP, sp - 4);
	m68k_set_reg(M68K_REG_PC, function);
	return 0;
}

int ptos_super_return(void)
{
	if (m68k_get_reg(NULL, M68K_REG_PC) != SUPER_RETURN + 2 || super->depth == super->floor) return 0;
	super->depth--;
	m68k_set_reg(M68K_REG_SR, super->frames[super->depth].sr);
	m68k_set_reg(M68K_REG_SP, super->frames[super->depth].sp);
	m68k_set_reg(M68K_REG_PC, super->frames[super->depth].pc);
	return 1;
}

static const char *guest_env(uint32_t address, size_t *size)
{
	if (!address) address = m68k_read_memory_32(ptos_process + BP_ENV);
	const char *s = ptos_guest_buffer(address, 2);
	uint32_t max = PTOS_RAM_END - address;
	if (max > PTOS_GUEST_ENV_MAX) max = PTOS_GUEST_ENV_MAX;
	for (uint32_t i = 0; i + 1 < max; i++)
		if (!s[i] && !s[i + 1]) { *size = i + 2; return s; }
	ptos_fault(address, "unterminated guest environment");
}

static char ptos_executable[PTOS_PATH_MAX];

int32_t ptos_set_executable(const char *path)
{
	int32_t drive;
	if (!path || !*path) return PTOS_EINVAL;
	if (path[1] == ':')
	{
		drive = ((unsigned char)path[0] & ~32) - 'A';
		path += 2;
	}
	else
		drive = ptos_call(GEMDOS_OP_DGETDRV, 0, 0, 0, 0);
	if (drive < 0 || drive >= 26 || !*path) return PTOS_EINVAL;
	char cwd[129] = {0};
	if (*path == '\\' || *path == '/')
		path++;
	else
	{
		int32_t status = ptos_call(GEMDOS_OP_DGETCWD, (intptr_t)cwd, drive + 1, sizeof(cwd), 0);
		if (status < 0) return status;
	}
	const char *directory = cwd + (cwd[0] == '\\' || cwd[0] == '/');
	int length = snprintf(ptos_executable, sizeof(ptos_executable), "%c:\\%s%s%s",
		'A' + (int)drive, directory, *directory ? "\\" : "", path);
	if (length < 0 || (size_t)length >= sizeof(ptos_executable)) return PTOS_ERANGE;
	for (char *p = ptos_executable; *p; p++)
		if (*p == '/') *p = '\\';
	return 0;
}

int32_t ptos_exec(uint16_t mode, uint32_t path, uint32_t tail, uint32_t env)
{
	uint16_t operation = mode & 0x7fff;
	int take, asynchronous;
	if (guest_go_mode(operation, &take, &asynchronous))
		return ptos_task_go(tail, take, asynchronous);
	if (operation != PTOS_EXEC_LOAD_GO && operation != PTOS_EXEC_LOAD_GO_ASYNC && operation != PTOS_EXEC_REPLACE && operation != PTOS_EXEC_LOAD &&
		operation != PTOS_EXEC_BASEPAGE && operation != PTOS_EXEC_BASEPAGE_FLAGS)
		return PTOS_ENOSYS;
	const char *command = ptos_guest_buffer(tail, 128);
	size_t env_size;
	const char *environment = guest_env(env, &env_size);
	if (operation == PTOS_EXEC_BASEPAGE || operation == PTOS_EXEC_BASEPAGE_FLAGS)
		return ptos_basepage(command, environment);
	const char *guest_path = ptos_guest_string(path, PTOS_PATH_MAX);
	char filename[PTOS_PATH_MAX];
	size_t n = strlen(guest_path);
	for (size_t i = 0; i <= n; i++) filename[i] = guest_path[i] == '/' ? '\\' : guest_path[i];
	if (operation == PTOS_EXEC_LOAD) return ptos_load(filename, command, environment);
	if (mode != operation) return PTOS_ENOSYS;
	ptos_launch_args args;
	int32_t result = ptos_capture_launch(filename, command, environment, env_size, env != 0, &args);
	if (result < 0) return result;
	char **options = NULL;
	result = ptos_is_prg(filename);
	if (!result)
		result = ptos_task_exec(operation, filename, args.argv, args.envp);
	else if (result > 0)
	{
		char *empty_env[] = {NULL};
		result = ptos_encode_guest_options(ptos_executable, filename, &args, &options);
		if (!result)
			result = ptos_task_exec(operation, ptos_executable, options,
				args.environment ? empty_env : args.envp);
	}
	free(options);
	free(args.envp);
	return result;
}
