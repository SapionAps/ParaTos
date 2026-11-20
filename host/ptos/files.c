#include <stdlib.h>
#include "host.h"
#include "guest.h"
#include "tasks.h"

enum { FILE_LIMIT = 256, STANDARD_FILES = 6 };
/* The in-place GO path is only used on legacy kernels. Its overlays share
 * one native PROCESS, so they need a local reference for each open file. */
typedef struct file_ref { unsigned int count; int owned; } file_ref;
typedef struct files {
	struct files *parent;
	file_ref *handles[FILE_LIMIT];
	int32_t saved[STANDARD_FILES];
} files;
static files root;
static files *current = &root;
static int legacy, legacy_auto;
typedef struct loaded_streams {
	struct loaded_streams *next;
	uint32_t bp, owner;
	int32_t handles[STANDARD_FILES];
} loaded_streams;
static loaded_streams *loaded;

struct ptos_clone_streams {
	size_t count;
	int restore;
	int32_t streams[STANDARD_FILES];
	int32_t inherited[];
};

static void close_streams(const int32_t handles[STANDARD_FILES])
{
	for (int i = 0; i < STANDARD_FILES; i++)
		if (handles[i] >= 0) ptos_call(GEMDOS_OP_FCLOSE, handles[i], 0, 0, 0);
}

static int32_t save_streams(int32_t handles[STANDARD_FILES])
{
	for (int i = 0; i < STANDARD_FILES; i++) handles[i] = PTOS_EIHNDL;
	for (int i = 0; i < STANDARD_FILES; i++)
	{
		int32_t r = ptos_call(GEMDOS_OP_FDUP, i, 0, 0, 0);
		if (r < 0 && r != PTOS_EIHNDL) { close_streams(handles); return r; }
		handles[i] = r;
	}
	return 0;
}

static int32_t restore_streams(const int32_t handles[STANDARD_FILES])
{
	int32_t result = 0;
	for (int i = 0; i < STANDARD_FILES; i++)
	{
		if (legacy || handles[i] < 0) ptos_call(GEMDOS_OP_FCLOSE, i, 0, 0, 0);
		if (handles[i] < 0) continue;
		int32_t r = ptos_call(GEMDOS_OP_FFORCE, i, handles[i], 0, 0);
		if (r < 0) result = r;
	}
	return result;
}

int32_t ptos_files_capture(uint32_t bp)
{
	loaded_streams *s = malloc(sizeof(*s));
	if (!s) return PTOS_ENSMEM;
	int32_t r = save_streams(s->handles);
	if (r < 0) { free(s); return r; }
	s->bp = bp; s->owner = ptos_process; s->next = loaded; loaded = s;
	return 0;
}

void ptos_files_drop(uint32_t bp)
{
	for (loaded_streams **at = &loaded; *at; at = &(*at)->next)
	{
		loaded_streams *s = *at;
		if (s->bp != bp) continue;
		*at = s->next;
		if (s->owner == ptos_process) close_streams(s->handles);
		free(s);
		return;
	}
}

static int private_stream(int32_t handle)
{
	for (loaded_streams *s = loaded; s; s = s->next)
	{
		if (s->owner != ptos_process) continue;
		for (int i = 0; i < STANDARD_FILES; i++)
			if (s->handles[i] == handle) return 1;
	}
	return 0;
}

static int32_t remember(int32_t handle, int owned)
{
	if (handle < 0) return handle;
	if (handle < STANDARD_FILES || handle >= FILE_LIMIT) return PTOS_ENHNDL;
	file_ref *ref = malloc(sizeof(*ref));
	if (!ref) return PTOS_ENSMEM;
	ref->count = 1; ref->owned = owned;
	current->handles[handle] = ref;
	return handle;
}

static void release(files *f)
{
	for (int i = STANDARD_FILES; i < FILE_LIMIT; i++)
	{
		file_ref *ref = f->handles[i];
		if (!ref) continue;
		if (!--ref->count)
		{
			if (ref->owned) ptos_call(GEMDOS_OP_FCLOSE, i, 0, 0, 0);
			free(ref);
		}
		f->handles[i] = NULL;
	}
}

int32_t ptos_files_init(void)
{
	/* Borrow existing bindings for the in-place overlay. */
	char scratch;
	for (int i = STANDARD_FILES; i < FILE_LIMIT; i++)
	{
		if (private_stream(i)) continue;
		int32_t r = ptos_call(GEMDOS_OP_FREAD, i, 0, (intptr_t)&scratch, 0);
		if (r == PTOS_EIHNDL) continue;
		if (r < 0 && r != -36) { release(&root); return r; }
		r = remember(i, 0);
		if (r < 0) { release(&root); return r; }
	}
	legacy = 1;
	return 0;
}

int32_t ptos_file_handle(int16_t handle)
{
	if (handle >= -3 && handle < STANDARD_FILES) return handle;
	if (handle < STANDARD_FILES || private_stream(handle)) return PTOS_EIHNDL;
	if (!legacy) return handle;
	if (handle >= FILE_LIMIT || !current->handles[handle]) return PTOS_EIHNDL;
	return handle;
}

int32_t ptos_file_open(uint16_t fn, const char *path, uint16_t mode)
{
	int32_t handle = ptos_call(fn, (intptr_t)path, mode, 0, 0);
	if (handle < 0 || !legacy) return handle;
	int32_t r = remember(handle, 1);
	if (r < 0) ptos_call(GEMDOS_OP_FCLOSE, handle, 0, 0, 0);
	return r;
}

int32_t ptos_file_dup(int16_t handle)
{
	if (ptos_file_handle(handle) == PTOS_EIHNDL) return PTOS_EIHNDL;
	int32_t copy = ptos_call(GEMDOS_OP_FDUP, handle, 0, 0, 0);
	if (copy < 0 || !legacy) return copy;
	int32_t r = remember(copy, 1);
	if (r < 0) ptos_call(GEMDOS_OP_FCLOSE, copy, 0, 0, 0);
	return r;
}

int32_t ptos_file_close(int16_t handle)
{
	int32_t r = ptos_file_handle(handle);
	if (r == PTOS_EIHNDL) return r;
	if (!legacy || handle < STANDARD_FILES) return ptos_call(GEMDOS_OP_FCLOSE, handle, 0, 0, 0);
	file_ref *ref = current->handles[handle];
	current->handles[handle] = NULL;
	if (--ref->count) return 0;
	free(ref);
	return ptos_call(GEMDOS_OP_FCLOSE, handle, 0, 0, 0);
}

int32_t ptos_file_force(int16_t standard, int16_t handle)
{
	if (standard < 0 || standard >= STANDARD_FILES ||
		(handle >= 0 && handle < STANDARD_FILES) || ptos_file_handle(handle) == PTOS_EIHNDL)
		return PTOS_EIHNDL;
	/* Legacy pTOS Fforce does not release the old destination binding. */
	ptos_call(GEMDOS_OP_FCLOSE, standard, 0, 0, 0);
	return ptos_call(GEMDOS_OP_FFORCE, standard, handle, 0, 0);
}

int32_t ptos_files_push(uint32_t bp)
{
	files *child = calloc(1, sizeof(*child));
	if (!child) return PTOS_ENSMEM;
	int32_t r;
	if (!legacy)
	{
		r = ptos_files_init();
		if (r < 0) { free(child); return r; }
		legacy_auto = 1;
	}
	r = save_streams(child->saved);
	if (r < 0) goto failed;
	for (loaded_streams *s = loaded; s; s = s->next)
	{
		if (s->bp != bp || s->owner != ptos_process) continue;
		r = restore_streams(s->handles);
		if (r < 0)
		{
			restore_streams(child->saved); close_streams(child->saved);
			goto failed;
		}
		ptos_files_drop(bp);
		break;
	}
	for (int i = STANDARD_FILES; i < FILE_LIMIT; i++)
		if ((child->handles[i] = current->handles[i])) child->handles[i]->count++;
	child->parent = current; current = child;
	return 0;
failed:
	free(child);
	if (legacy_auto && current == &root)
	{
		release(&root);
		legacy = legacy_auto = 0;
	}
	return r;
}

int32_t ptos_files_pop(void)
{
	files *child = current;
	if (!child->parent) return PTOS_EINVAL;
	int32_t result = restore_streams(child->saved);
	close_streams(child->saved);
	release(child);
	current = child->parent; free(child);
	if (legacy_auto && current == &root)
	{
		release(&root);
		legacy = legacy_auto = 0;
	}
	return result;
}

void ptos_files_cleanup(void)
{
	while (current != &root) ptos_files_pop();
	release(&root);
	legacy = legacy_auto = 0;
}

/* Native descriptor inheritance also copies the emulator's private staged
 * stream duplicates. Record their numbers at launch, install just the chosen
 * image's streams in the child and drop every inherited private duplicate. */
int32_t ptos_clone_streams_prepare(uint32_t bp, ptos_clone_streams **out)
{
	size_t count = 0;
	for (loaded_streams *s = loaded; s; s = s->next)
		if (s->owner == ptos_process) count += STANDARD_FILES;
	if (!count) return 0;
	ptos_clone_streams *clone = malloc(sizeof(*clone) + count * sizeof(*clone->inherited));
	if (!clone) return PTOS_ENSMEM;
	clone->count = 0;
	clone->restore = 0;
	for (loaded_streams *s = loaded; s; s = s->next)
	{
		if (s->owner != ptos_process) continue;
		for (int i = 0; i < STANDARD_FILES; i++)
		{
			if (s->bp == bp) clone->streams[i] = s->handles[i];
			if (s->handles[i] >= 0) clone->inherited[clone->count++] = s->handles[i];
		}
		if (s->bp == bp) clone->restore = 1;
	}
	*out = clone;
	return 0;
}

int32_t ptos_clone_streams_install(const ptos_clone_streams *streams)
{
	if (!streams) return 0;
	int32_t result = streams->restore ? restore_streams(streams->streams) : 0;
	for (size_t i = 0; i < streams->count; i++)
		ptos_call(GEMDOS_OP_FCLOSE, streams->inherited[i], 0, 0, 0);
	return result;
}
