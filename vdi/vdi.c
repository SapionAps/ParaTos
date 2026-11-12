#include <stddef.h>
#include <stdlib.h>
#include "vdi.h"

/* Both host memory implementations provide the Musashi accessors. */
unsigned int m68k_read_memory_16(unsigned int address);
unsigned int m68k_read_memory_32(unsigned int address);
void m68k_write_memory_16(unsigned int address, unsigned int value);
void m68k_write_memory_32(unsigned int address, unsigned int value);

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(paratos_vdi_control) == 28, "native VDI control size");
_Static_assert(offsetof(paratos_vdi_control, args) == 16, "native VDI pointer alignment");
_Static_assert(sizeof(paratos_vdi_pb) == 20, "native VDI parameter block size");
_Static_assert(sizeof(paratos_vdi_fdb) == 20, "native MFDB size");
#endif

typedef struct bitmap {
	paratos_vdi_fdb native;
	uint32_t descriptor, address, bytes;
	unsigned int unit;
	void *copy;
} bitmap;

static int range(const paratos_vdi_host *host, uint32_t address, uint32_t bytes, int writing)
{
	return !bytes || (!(address & 1) && bytes <= UINT32_MAX - address &&
		host->range(address, bytes, writing));
}

/* Output counts are operation results, never guest-supplied capacities. */
static int32_t shape(paratos_vdi_control *c, int *ni, int *np)
{
	int input = 0, points = 0;
	*ni = *np = 0;
	if (c->nintin < 0 || c->nptsin < 0) return VDI_EINVAL;
	/* Classic bindings leave unused subcodes behind. The native client
	 * requires them zeroed. GDP and the Bezier polyline selector are real
	 * variants; never silently reinterpret those as ordinary operations. */
	if (c->code == VDI_POLYLINE && c->subcode == 13) return VDI_ENOSYS;
	if (c->code != VDI_GDP) c->subcode = 0;
	switch (c->code)
	{
	case VDI_OPEN: input = 11; *ni = 45; *np = 6; break;
	case VDI_CLOSE: break;
	case VDI_POLYLINE:
		if (c->nptsin < 2 || c->nptsin > VDI_MAX_POINTS) return VDI_EINVAL;
		points = c->nptsin; break;
	case VDI_TEXT:
		if (c->nintin > VDI_MAX_INTEGERS) return VDI_EINVAL;
		input = c->nintin; points = 1; break;
	case VDI_GDP:
		if (c->subcode != 1) return VDI_ENOSYS;
		points = 2; break;
	case VDI_TEXT_HEIGHT: points = 1; *np = 2; break;
	case VDI_LINE_WIDTH: points = 1; *np = 1; break;
	case VDI_LINE_TYPE: case VDI_LINE_COLOR: case VDI_TEXT_COLOR:
	case VDI_FILL_INTERIOR: case VDI_FILL_STYLE: case VDI_FILL_COLOR:
	case VDI_WRITE_MODE: input = 1; *ni = 1; break;
	case VDI_TEXT_ATTRIBUTES: *ni = 6; *np = 2; break;
	case VDI_INQUIRE: input = 1; *ni = 45; *np = 6; break;
	case VDI_PIXEL: points = 1; *ni = 2; break;
	case VDI_COPY: input = 1; points = 4; break;
	case VDI_TRANSFORM: break;
	case VDI_TRANSPARENT: input = 3; points = 4; break;
	case VDI_LINE_PATTERN: input = 1; break;
	case VDI_RECTANGLE: points = 2; break;
	case VDI_CLIP: input = 1; points = 2; break;
	/* Font chains and 68k callback addresses cannot be native pointers. */
	default: return VDI_ENOSYS;
	}
	return c->nintin == input && c->nptsin == points ? 0 : VDI_EINVAL;
}

static void read_words(uint32_t address, int16_t *words, unsigned int count)
{
	for (unsigned int i = 0; i < count; i++) words[i] = m68k_read_memory_16(address + i * 2);
}

static void write_words(uint32_t address, const int16_t *words, unsigned int count)
{
	for (unsigned int i = 0; i < count; i++) m68k_write_memory_16(address + i * 2, (uint16_t)words[i]);
}

static int32_t bitmap_shape(bitmap *b, uint32_t descriptor, const paratos_vdi_host *host, int writing, int update)
{
	if (!range(host, descriptor, 20, update)) return VDI_EIMBA;
	b->descriptor = descriptor;
	b->address = m68k_read_memory_32(descriptor);
	paratos_vdi_fdb *f = &b->native;
	f->width = m68k_read_memory_16(descriptor + 4);
	f->height = m68k_read_memory_16(descriptor + 6);
	f->stride = m68k_read_memory_16(descriptor + 8);
	f->standard = m68k_read_memory_16(descriptor + 10);
	f->planes = m68k_read_memory_16(descriptor + 12);
	for (unsigned int i = 0; i < 3; i++) f->reserved[i] = m68k_read_memory_16(descriptor + 14 + i * 2);
	if (!b->address) return 0; /* A screen MFDB has no userspace bitmap. */
	if (f->width <= 0 || f->height <= 0 || f->stride <= 0 || f->planes <= 0 || f->planes > 32 ||
		(f->standard != 0 && f->standard != 1) || f->reserved[0] || f->reserved[1] || f->reserved[2] ||
		(uint32_t)f->stride * 16 < (uint32_t)f->width) return VDI_EINVAL;
	b->unit = !f->standard && f->planes >= 16 ? (unsigned int)f->planes / 8 : 2;
	if (b->unit != 2 && b->unit != 4) return VDI_EINVAL;
	uint64_t stride = !f->standard && f->planes >= 16 ? (uint64_t)f->width * b->unit :
		(uint64_t)f->stride * 2 * f->planes;
	uint64_t bytes = stride * f->height;
	if (bytes > VDI_MAX_BITMAP) return VDI_EINVAL;
	b->bytes = bytes;
	return range(host, b->address, b->bytes, writing) ? 0 : VDI_EIMBA;
}

static int32_t snapshot(bitmap *b)
{
	if (!b->bytes) return 0;
	b->copy = malloc(b->bytes);
	if (!b->copy) return VDI_ENSMEM;
	b->native.address = b->copy;
	for (uint32_t at = 0; at < b->bytes; at += b->unit)
		if (b->unit == 2) ((uint16_t *)b->copy)[at / 2] = m68k_read_memory_16(b->address + at);
		else ((uint32_t *)b->copy)[at / 4] = m68k_read_memory_32(b->address + at);
	return 0;
}

static void commit_bitmap(const bitmap *b)
{
	for (uint32_t at = 0; at < b->bytes; at += b->unit)
		if (b->unit == 2) m68k_write_memory_16(b->address + at, ((const uint16_t *)b->copy)[at / 2]);
		else m68k_write_memory_32(b->address + at, ((const uint32_t *)b->copy)[at / 4]);
}

int32_t paratos_vdi_marshal(uint32_t address, const paratos_vdi_host *host)
{
	if (!range(host, address, 20, 0)) return VDI_EIMBA;
	uint32_t pointers[5];
	for (unsigned int i = 0; i < 5; i++) pointers[i] = m68k_read_memory_32(address + i * 4);
	if (!range(host, pointers[0], 24, 1)) return VDI_EIMBA;
	int16_t original[12];
	read_words(pointers[0], original, 12);
	paratos_vdi_control control = {.code = original[0], .nptsin = original[1],
		.nintin = original[3], .subcode = original[5], .handle = original[6]};
	int ni, np;
	int32_t result = shape(&control, &ni, &np);
	if (result < 0) return result;
	if (!range(host, pointers[1], control.nintin * 2u, 0) ||
		!range(host, pointers[2], control.nptsin * 4u, 0) ||
		!range(host, pointers[3], ni * 2u, 1) || !range(host, pointers[4], np * 4u, 1)) return VDI_EIMBA;
	int16_t input[VDI_MAX_INTEGERS] = {0}, points[VDI_MAX_POINTS * 2] = {0};
	int16_t output[45] = {0}, outpoints[12] = {0};
	read_words(pointers[1], input, control.nintin);
	read_words(pointers[2], points, control.nptsin * 2);
	paratos_vdi_pb block = {&control, input, points, output, outpoints};
	bitmap source = {0}, destination = {0};
	int operation = control.code;
	int raster = operation == VDI_COPY || operation == VDI_TRANSFORM || operation == VDI_TRANSPARENT;
	if (raster)
	{
		/* Classic pointers start at byte 14; native pointers start at 16. */
		result = bitmap_shape(&source, m68k_read_memory_32(pointers[0] + 14), host, 0, 0);
		if (result >= 0) result = bitmap_shape(&destination, m68k_read_memory_32(pointers[0] + 18), host, 1, operation == VDI_TRANSFORM);
		if (result >= 0) result = snapshot(&source);
		if (result >= 0) result = snapshot(&destination);
		if (result < 0) goto done;
		control.args.raster.source = &source.native;
		control.args.raster.destination = &destination.native;
	}
	result = host->call(&block);
	if (result < 0) goto done;
	if (control.nintout < 0 || control.nintout > ni || control.nptsout < 0 || control.nptsout > np ||
		(operation == VDI_TRANSFORM && (destination.native.standard < 0 || destination.native.standard > 1)))
	{
		result = VDI_EINVAL; goto done;
	}
	/* All input and writable ranges were checked before the host operation.
 * Commit only the descriptor's defined output; never expose native pointers. */
	if (raster) commit_bitmap(&destination);
	if (operation == VDI_TRANSFORM)
		m68k_write_memory_16(destination.descriptor + 10, destination.native.standard);
	write_words(pointers[3], output, control.nintout);
	write_words(pointers[4], outpoints, control.nptsout * 2);
	/* Input/pointer/filler words can alias valid guest output buffers. */
	m68k_write_memory_16(pointers[0] + 4, (uint16_t)control.nptsout);
	m68k_write_memory_16(pointers[0] + 8, (uint16_t)control.nintout);
	m68k_write_memory_16(pointers[0] + 12, (uint16_t)control.handle);
done:
	free(source.copy); free(destination.copy);
	return result;
}
