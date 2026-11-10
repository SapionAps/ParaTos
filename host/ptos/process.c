#include <stdlib.h>
#include <string.h>
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"

int ptos_finished;
uint16_t ptos_status;

typedef struct parent {
	struct parent *next;
	void *cpu;
	uint32_t bp;
	unsigned int super_depth, super_floor;
	int32_t drive;
	uint32_t directories;
	char cwd[32][129];
} parent;
static parent *parents;
static unsigned int parent_depth;

#define SUPER_RETURN 0x8000u
static struct { uint32_t pc, sr, sp; } super_frames[16];
static unsigned int super_depth, super_floor;

static int32_t save_directories(parent *p)
{
	p->drive = ptos_call(GEMDOS_OP_DGETDRV, 0, 0, 0, 0);
	if (p->drive < 0) return p->drive;
	for (unsigned int i = 0; i < 32; i++)
	{
		/* Dgetpath returns a drive-relative path; restore it from root. */
		p->cwd[i][0] = '\\';
		int32_t r = ptos_call(GEMDOS_OP_DGETPATH, (intptr_t)(p->cwd[i] + 1), i + 1, 0, 0);
		if (r == -46 || r == -34) continue; /* absent drive or unmounted medium */
		if (r < 0) return r;
		p->directories |= UINT32_C(1) << i;
	}
	return 0;
}

static int32_t restore_directories(const parent *p)
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

void ptos_process_cleanup(void)
{
	while (parents)
	{
		parent *p = parents;
		parents = p->next;
		restore_directories(p);
		free(p->cpu); free(p);
	}
	parent_depth = super_depth = super_floor = 0;
}

void ptos_start_cpu(uint32_t bp)
{
	/* A nested guest inherits the unused top of the emulated supervisor
	 * stack. Resetting it would overwrite a parent's active Supexec frame. */
	uint32_t supervisor = parent_depth ? m68k_get_reg(NULL, M68K_REG_ISP) : PTOS_ALLOC_START;
	uint32_t stack = m68k_read_memory_32(bp + BP_HIGH) & ~3u;
	stack -= 8;
	m68k_write_memory_32(stack, 0);
	m68k_write_memory_32(stack + 4, bp);
	m68k_set_reg(M68K_REG_SR, 0x0300);
	m68k_set_reg(M68K_REG_ISP, supervisor);
	m68k_set_reg(M68K_REG_SP, stack);
	/* TOS startup uses A0 != 0 to identify an accessory. An ordinary
	 * overlay must not inherit that register from its parent. */
	m68k_set_reg(M68K_REG_A0, 0);
	m68k_set_reg(M68K_REG_PC, m68k_read_memory_32(bp + BP_TEXT));
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
	int32_t result = save_directories(p);
	if (result < 0) { free(p->cpu); free(p); return result; }
	m68k_get_context(p->cpu);
	p->bp = ptos_process; p->super_depth = super_depth;
	p->super_floor = super_floor;
	p->next = parents; parents = p; parent_depth++;
	if (take_ownership) ptos_take_image(bp);
	m68k_write_memory_32(bp + BP_PARENT, ptos_process);
	ptos_process = bp;
	super_floor = super_depth;
	ptos_start_cpu(bp);
	return 0;
}

void ptos_terminate(uint16_t status)
{
	ptos_free_owned(ptos_process);
	if (parents)
	{
		parent *p = parents;
		ptos_process = p->bp; super_depth = p->super_depth;
		super_floor = p->super_floor;
		m68k_set_context(p->cpu);
		m68k_set_reg(M68K_REG_D0, status);
		parents = p->next; parent_depth--;
		int32_t restored = restore_directories(p);
		free(p->cpu); free(p);
		if (restored < 0) ptos_fault(ptos_process, "cannot restore overlay parent directory");
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
	if (super_depth == sizeof(super_frames) / sizeof(super_frames[0])) return PTOS_ENSMEM;
	super_frames[super_depth].pc = m68k_get_reg(NULL, M68K_REG_PC);
	super_frames[super_depth].sr = m68k_get_reg(NULL, M68K_REG_SR);
	super_frames[super_depth].sp = m68k_get_reg(NULL, M68K_REG_SP);
	super_depth++;
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
	if (m68k_get_reg(NULL, M68K_REG_PC) != SUPER_RETURN + 2 || super_depth == super_floor) return 0;
	super_depth--;
	m68k_set_reg(M68K_REG_SR, super_frames[super_depth].sr);
	m68k_set_reg(M68K_REG_SP, super_frames[super_depth].sp);
	m68k_set_reg(M68K_REG_PC, super_frames[super_depth].pc);
	return 1;
}

static const char *guest_env(uint32_t address)
{
	if (!address) address = m68k_read_memory_32(ptos_process + BP_ENV);
	const char *s = ptos_guest_buffer(address, 2);
	uint32_t max = PTOS_RAM_END - address;
	if (max > 32766) max = 32766;
	for (uint32_t i = 0; i + 1 < max; i++) if (!s[i] && !s[i + 1]) return s;
	ptos_fault(address, "unterminated guest environment");
}

int32_t ptos_exec(uint16_t mode, uint32_t path, uint32_t tail, uint32_t env)
{
	uint16_t operation = mode & 0x7fff;
	if (operation == PTOS_EXEC_GO || operation == PTOS_EXEC_GO_FREE)
		return ptos_go(tail, operation == PTOS_EXEC_GO_FREE);
	if (operation != PTOS_EXEC_LOAD_GO && operation != PTOS_EXEC_REPLACE && operation != PTOS_EXEC_LOAD &&
		operation != PTOS_EXEC_BASEPAGE && operation != PTOS_EXEC_BASEPAGE_FLAGS)
		return PTOS_ENOSYS;
	const char *command = ptos_guest_buffer(tail, 128);
	const char *environment = guest_env(env);
	if (operation == PTOS_EXEC_BASEPAGE || operation == PTOS_EXEC_BASEPAGE_FLAGS)
		return ptos_basepage(command, environment);
	const char *guest_path = ptos_guest_string(path, 1024);
	char filename[1024];
	size_t n = strlen(guest_path);
	for (size_t i = 0; i <= n; i++) filename[i] = guest_path[i] == '/' ? '\\' : guest_path[i];
	if (operation == PTOS_EXEC_LOAD) return ptos_load(filename, command, environment);
	/* Includes guest-to-guest modes 0 and 200: let native Pexec/binfmt
	 * select another Paratos instance. Never turn replacement into spawn. */
	return ptos_call(GEMDOS_OP_PEXEC, mode, (intptr_t)filename, (intptr_t)command, (intptr_t)environment);
}
