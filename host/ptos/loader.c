#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "memory.h"
#include "guest.h"

typedef struct block {
	struct block *next;
	uint32_t start, size, owner;
	int image;
} block;

static block *blocks;
uint32_t ptos_process;

void ptos_allocator_destroy(void)
{
	while (blocks)
	{
		block *next = blocks->next;
		free(blocks);
		blocks = next;
	}
}

void ptos_allocator_init(void)
{
	ptos_allocator_destroy();
	ptos_process = 0;
}

static uint32_t gap(uint32_t start, uint32_t end, uint32_t need, uint32_t *largest)
{
	if (end - start > *largest) *largest = end - start;
	return need && end - start >= need ? start : 0;
}

int32_t ptos_alloc(int32_t size)
{
	if (size != -1 && (size <= 0 || (uint32_t)size > PTOS_RAM_END - PTOS_ALLOC_START))
		return PTOS_ENSMEM;
	uint32_t need = size == -1 ? 0 : ((uint32_t)size + 3) & ~3u;
	uint32_t start = PTOS_ALLOC_START, largest = 0, found = 0;
	block **at = &blocks;
	while (*at)
	{
		if ((found = gap(start, (*at)->start, need, &largest))) break;
		start = (*at)->start + (*at)->size;
		at = &(*at)->next;
	}
	if (!found) found = gap(start, PTOS_RAM_END, need, &largest);
	if (size == -1) return largest;
	if (!found) return PTOS_ENSMEM;
	block *b = malloc(sizeof(*b));
	if (!b) return PTOS_ENSMEM;
	b->start = found; b->size = need; b->owner = ptos_process;
	b->image = 0;
	b->next = *at; *at = b;
	memset(ptos_guest_buffer(found, need), 0, need);
	return found;
}

int32_t ptos_free(uint32_t address)
{
	for (block **at = &blocks; *at; at = &(*at)->next)
	{
		if ((*at)->start != address) continue;
		block *old = *at;
		*at = old->next;
		free(old);
		return 0;
	}
	return PTOS_EIMBA;
}

void ptos_free_owned(uint32_t owner)
{
	for (block **at = &blocks; *at; )
	{
		if ((*at)->owner != owner) { at = &(*at)->next; continue; }
		block *old = *at;
		*at = old->next;
		free(old);
	}
}

int32_t ptos_shrink(uint32_t address, int32_t size)
{
	if (size == 0) return ptos_free(address);
	if (size < 0 || (uint32_t)size > PTOS_RAM_END) return PTOS_EINVAL;
	uint32_t need = ((uint32_t)size + 3) & ~3u;
	for (block *b = blocks; b; b = b->next)
	{
		if (b->start != address) continue;
		if (need > b->size) return -67; /* EGSBF */
		b->size = need;
		if (b->image) m68k_write_memory_32(address + BP_HIGH, address + need);
		return 0;
	}
	return PTOS_EIMBA;
}

void ptos_take_image(uint32_t bp)
{
	uint32_t env = m68k_read_memory_32(bp + BP_ENV);
	for (block *b = blocks; b; b = b->next)
		if (b->start == bp || b->start == env) b->owner = bp;
}

int ptos_image_valid(uint32_t bp)
{
	for (block *b = blocks; b; b = b->next)
		if (b->start == bp && b->size >= BP_SIZE &&
			m68k_read_memory_32(bp + BP_LOW) == bp &&
			m68k_read_memory_32(bp + BP_HIGH) <= bp + b->size)
			return 1;
	return 0;
}

int32_t ptos_basepage(const char *tail, const char *environment)
{
	uint32_t env_size = 0;
	while (env_size < 32764 && (environment[env_size] || environment[env_size + 1])) env_size++;
	if (env_size == 32764) return PTOS_ERANGE;
	env_size += 2;
	int32_t env = ptos_alloc(env_size);
	if (env < 0) return env;
	memcpy(ptos_guest_buffer(env, env_size), environment, env_size);
	int32_t size = ptos_alloc(-1);
	int32_t bp = size >= BP_SIZE + 16 ? ptos_alloc(size) : PTOS_ENSMEM;
	if (bp < 0) { ptos_free(env); return bp; }
	m68k_write_memory_32(bp + BP_LOW, bp);
	m68k_write_memory_32(bp + BP_HIGH, bp + size);
	m68k_write_memory_32(bp + BP_DTA, bp + BP_TAIL);
	m68k_write_memory_32(bp + BP_ENV, env);
	m68k_write_memory_32(bp + BP_PARENT, ptos_process);
	for (block *b = blocks; b; b = b->next) if (b->start == (uint32_t)bp) b->image = 1;
	uint32_t len = (uint8_t)tail[0];
	if (len > 127) { ptos_free(bp); ptos_free(env); return PTOS_EINVAL; }
	m68k_write_memory_8(bp + BP_TAIL, len);
	memcpy(ptos_guest_buffer(bp + BP_TAIL + 1, len), tail + 1, len);
	/* A 127-byte ARGV marker has no room for an extra basepage terminator. */
	if (len < 127) m68k_write_memory_8(bp + BP_TAIL + 1 + len, 0);
	return bp;
}

static uint32_t be32(const unsigned char *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int32_t read_exact(int32_t handle, void *buffer, uint32_t count)
{
	unsigned char *out = buffer;
	while (count)
	{
		int32_t n = ptos_call(GEMDOS_OP_FREAD, handle, count, (intptr_t)out, 0);
		if (n < 0) return n;
		if (!n || (uint32_t)n > count) return PTOS_EPLFMT;
		out += n; count -= n;
	}
	return 0;
}

int32_t ptos_load(const char *path, const char *tail, const char *environment)
{
	unsigned char hdr[28], fix[4];
	int32_t result = PTOS_EPLFMT, bp = 0;
	int32_t handle = ptos_call(GEMDOS_OP_FOPEN, (intptr_t)path, 0, 0, 0);
	if (handle < 0) return handle;
	result = read_exact(handle, hdr, sizeof(hdr));
	if (result < 0) goto done;
	result = PTOS_EPLFMT;
	if (hdr[0] != 0x60 || hdr[1] != 0x1a) goto done;
	uint32_t text = be32(hdr + 2), data = be32(hdr + 6), bss = be32(hdr + 10), symbols = be32(hdr + 14);
	if (text < 2 || text > PTOS_RAM_END || data > PTOS_RAM_END - text || bss > PTOS_RAM_END - text - data) goto done;
	bp = ptos_basepage(tail, environment);
	if (bp < 0) { result = bp; bp = 0; goto done; }
	uint32_t base = bp + BP_SIZE, high = m68k_read_memory_32(bp + BP_HIGH);
	if (high - base < 4096 || text + data + bss > high - base - 4096) { result = PTOS_ENSMEM; goto done; }
	m68k_write_memory_32(bp + BP_TEXT, base); m68k_write_memory_32(bp + BP_TLEN, text);
	m68k_write_memory_32(bp + BP_DATA, base + text); m68k_write_memory_32(bp + BP_DLEN, data);
	m68k_write_memory_32(bp + BP_BSS, base + text + data); m68k_write_memory_32(bp + BP_BLEN, bss);
	result = read_exact(handle, ptos_guest_buffer(base, text + data), text + data);
	if (result < 0) goto done;
	result = PTOS_EPLFMT;
	if (!(hdr[26] || hdr[27]))
	{
		if (symbols > INT32_MAX) goto done;
		result = ptos_call(GEMDOS_OP_FSEEK, symbols, handle, 1, 0);
		if (result < 0) goto done;
		result = read_exact(handle, fix, 4);
		if (result < 0) goto done;
		result = PTOS_EPLFMT;
		uint32_t offset = be32(fix);
		while (offset)
		{
			if (text + data < 4 || offset > text + data - 4 || (offset & 1)) goto done;
			m68k_write_memory_32(base + offset, m68k_read_memory_32(base + offset) + base);
			unsigned char delta;
			do {
				int32_t r = read_exact(handle, &delta, 1);
				if (r < 0) { result = r; goto done; }
				uint32_t step = delta == 1 ? 254 : (uint32_t)delta;
				if (offset > UINT32_MAX - step) goto done;
				offset += step;
			} while (delta == 1);
			if (!delta) break;
		}
	}
	result = bp;
done:
	{
		int32_t closed = ptos_call(GEMDOS_OP_FCLOSE, handle, 0, 0, 0);
		if (result >= 0 && closed < 0) result = closed;
	}
	if (result < 0 && bp > 0) { ptos_free(m68k_read_memory_32(bp + BP_ENV)); ptos_free(bp); }
	return result;
}
