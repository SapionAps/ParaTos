#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "memory.h"
#include "guest.h"
#include "filesystem.h"

_Static_assert(offsetof(ptos_native_dta, size) == 28, "native DTA size offset");
_Static_assert(offsetof(ptos_native_dta, name) == 32, "native DTA name offset");
_Static_assert(sizeof(ptos_native_dta) == 48, "native DTA size");
_Static_assert(offsetof(ptos_native_xattr, size) == 20, "native XATTR size offset");
_Static_assert(offsetof(ptos_native_xattr, stamp) == 32, "native XATTR date offset");
_Static_assert(sizeof(ptos_native_xattr) == 56, "native XATTR size");

typedef struct search {
	struct search *next;
	uint32_t owner, address;
	int started;
	ptos_native_dta native;
} search;
static search *searches;
/* The pTOS protected DTA table has eight slots per native process. Never
 * start a ninth cursor: it would silently evict a suspended guest's search. */
#define NATIVE_SEARCH_LIMIT 8

static unsigned int active_searches(void)
{
	unsigned int count = 0;
	for (search *p = searches; p; p = p->next) count += p->started != 0;
	return count;
}
/* Never used for Fsfirst. Fsnext on an unstarted guest DTA must not reuse
 * kernel search state belonging to a freed/reallocated native buffer. */
static ptos_native_dta empty_dta;
static uint32_t pending_dta;
static int pending_restore;

static int32_t restore_native_dta(uint32_t address)
{
	int32_t result = ptos_call(GEMDOS_OP_FSETDTA, (intptr_t)address, 0, 0, 0);
	pending_restore = result < 0;
	if (pending_restore) pending_dta = address;
	return result;
}

static int32_t repair_native_dta(void)
{
	return pending_restore ? restore_native_dta(pending_dta) : 0;
}

/* Exhaust through Fsnext: both the protected DTA cache and a pluggable
 * filesystem's directory cookie must be released before freeing the buffer.
 * A failing Fsfirst can clear only the former and leak the latter. */
static int32_t cancel_search(search *p)
{
	int32_t result = repair_native_dta();
	if (result < 0 || !p->started) return result;
	int32_t old = ptos_call(GEMDOS_OP_FGETDTA, 0, 0, 0, 0);
	if (old < 0) return old;
	result = ptos_call(GEMDOS_OP_FSETDTA, (intptr_t)&p->native, 0, 0, 0);
	if (result < 0) return result;
	do { result = ptos_call(GEMDOS_OP_FSNEXT, 0, 0, 0, 0); } while (result >= 0);
	if (result == PTOS_ENMFIL) p->started = 0;
	int32_t restored = restore_native_dta((uint32_t)old);
	/* Other errors do not prove that every native cursor has been released.
	 * Keep the context alive and retry later rather than losing its address. */
	if (result != PTOS_ENMFIL) return result;
	return restored;
}

static search *find_search(uint32_t address)
{
	for (search *p = searches; p; p = p->next)
		if (p->owner == ptos_process && p->address == address) return p;
	return NULL;
}

/* An inactive context has no native cursor to preserve. If restoration is
 * still pending, retire it for the restoration-aware cleanup path instead. */
static int32_t search_error(search *p, int32_t result)
{
	if (p && !p->started)
	{
		p->owner = 0;
		if (!pending_restore)
			for (search **at = &searches; *at; at = &(*at)->next)
				if (*at == p) { *at = p->next; free(p); break; }
	}
	return result;
}

int32_t ptos_search(uint16_t function, const char *path, uint16_t attribute)
{
	uint32_t address = m68k_read_memory_32(ptos_process + BP_DTA);
	ptos_guest_buffer(address, 44);
	int32_t repaired = repair_native_dta();
	if (repaired < 0) return repaired;
	/* Temporary cleanup failures must not permanently occupy the cursor
	 * budget. Retry retired contexts before admitting another search. */
	ptos_filesystem_drop(0);
	repaired = repair_native_dta();
	if (repaired < 0) return repaired;
	search *p = find_search(address);
	if (function == GEMDOS_OP_FSFIRST && (!p || !p->started) &&
		active_searches() >= NATIVE_SEARCH_LIMIT) return PTOS_ENSMEM;
	if (function == GEMDOS_OP_FSFIRST && !p)
	{
		p = calloc(1, sizeof(*p));
		if (!p) return PTOS_ENSMEM;
		p->owner = ptos_process; p->address = address;
		p->next = searches; searches = p;
	}
	if (function == GEMDOS_OP_FSFIRST && p->started)
	{
		int32_t cancelled = cancel_search(p);
		if (cancelled < 0) return search_error(p, cancelled);
	}
	ptos_native_dta *native = p && (function == GEMDOS_OP_FSFIRST || p->started)
		? &p->native : &empty_dta;
	int32_t old = ptos_call(GEMDOS_OP_FGETDTA, 0, 0, 0, 0);
	if (old < 0) return search_error(p, old);
	int32_t result = ptos_call(GEMDOS_OP_FSETDTA, (intptr_t)native, 0, 0, 0);
	if (result < 0) return search_error(p, result);
	if (function == GEMDOS_OP_FSFIRST) memset(native, 0, sizeof(*native));
	result = ptos_call(function, (intptr_t)path, attribute, 0, 0);
	int32_t restored = restore_native_dta((uint32_t)old);
	if (p && function == GEMDOS_OP_FSFIRST) p->started = result >= 0;
	if (p && function == GEMDOS_OP_FSNEXT && result == PTOS_ENMFIL) p->started = 0;
	/* Preserve the operation's error, including native ENOSYS. */
	if (result < 0) return search_error(p, result);
	if (restored < 0) return search_error(p, restored);
	/* Native search state stays private and keyed by owner/DTA address.
	 * The public result is the classic 44-byte 68k layout. */
	for (unsigned int i = 0; i < 21; i++) m68k_write_memory_8(address + i, 0);
	m68k_write_memory_8(address + 21, native->attribute);
	m68k_write_memory_16(address + 22, native->time);
	m68k_write_memory_16(address + 24, native->date);
	m68k_write_memory_32(address + 26, native->size);
	for (unsigned int i = 0; i < sizeof(native->name); i++)
		m68k_write_memory_8(address + 30 + i, (uint8_t)native->name[i]);
	return result;
}

int32_t ptos_disk_free(uint32_t buffer, uint16_t drive)
{
	ptos_guest_buffer(buffer, 16);
	uint32_t values[4] = {0};
	int32_t result = ptos_call(GEMDOS_OP_DFREE, (intptr_t)values, drive, 0, 0);
	if (result >= 0)
		for (unsigned int i = 0; i < 4; i++) m68k_write_memory_32(buffer + i * 4, values[i]);
	return result;
}

int32_t ptos_xattr(uint16_t flag, const char *path, uint32_t buffer)
{
	ptos_guest_buffer(buffer, 52);
	ptos_native_xattr value = {0};
	int32_t result = ptos_call(GEMDOS_OP_FXATTR, flag, (intptr_t)path, (intptr_t)&value, 0);
	if (result < 0) return result;
	m68k_write_memory_16(buffer, value.mode);
	m68k_write_memory_32(buffer + 2, value.index);
	const uint16_t short_fields[] = {value.dev, value.rdev, value.nlink, value.uid, value.gid};
	for (unsigned int i = 0; i < 5; i++) m68k_write_memory_16(buffer + 6 + i * 2, short_fields[i]);
	m68k_write_memory_32(buffer + 16, value.size);
	m68k_write_memory_32(buffer + 20, value.blksize);
	m68k_write_memory_32(buffer + 24, value.blocks);
	for (unsigned int i = 0; i < 3; i++)
	{
		m68k_write_memory_16(buffer + 28 + i * 4, value.stamp[i].time);
		m68k_write_memory_16(buffer + 30 + i * 4, value.stamp[i].date);
	}
	m68k_write_memory_16(buffer + 40, value.attribute);
	m68k_write_memory_16(buffer + 42, value.reserved1);
	m68k_write_memory_32(buffer + 44, value.reserved2[0]);
	m68k_write_memory_32(buffer + 48, value.reserved2[1]);
	return result;
}

void ptos_filesystem_drop(uint32_t owner)
{
	for (search **at = &searches; *at; )
	{
		search *p = *at;
		if (p->owner != owner) { at = &p->next; continue; }
		/* Keep the buffer alive for a later retry if native restoration fails.
		 * Retire its owner so a reused guest basepage cannot claim it. */
		if (cancel_search(p) < 0) { p->owner = 0; at = &p->next; continue; }
		*at = p->next; free(p);
	}
}

void ptos_filesystem_cleanup(void)
{
	/* Fsnext on empty_dta can also leave a pending native restoration. */
	if (repair_native_dta() < 0) return;
	for (search **at = &searches; *at; )
	{
		search *p = *at;
		if (cancel_search(p) < 0) { p->owner = 0; at = &p->next; continue; }
		*at = p->next; free(p);
	}
}
