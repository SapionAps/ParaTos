#pragma once
#include "common.h"
#include "gemdos/path.h"

// Allocates a path name pointing to a valid Unix file name, mapping the source path, expanding any hashed 8.3
// path components on the way and/or expanding drive names. Returns NULL if the path name could not be expanded.
// The caller must deallocate the returned path using free().
char* tos_path_to_unix(char* tos_path);

// Wrapper around tos_path_to_unix that takes an emulated memory location instead of a char*
// As with tos_path_to_unix, the caller must deallocate the returned path using free().
char* read_path(emuptr32_t tos_path);
