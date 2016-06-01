#pragma once
#define PATH_SEP(c) ((c)=='/' || (c)=='\\')
#define END_OF_NAME(c) ((c)==0 || PATH_SEP(c))

// Utility function that takes a source file name and converts it to a 8.3 TOS file name.
// Tos domain processes will also get the name converted to uppercase.
// The destination buffer has to be 13 bytes large.
const char* filename8_3(char* dest, const char* source);

// Allocates a path name pointing to a valid Unix file name, mapping the source path, expanding any hashed 8.3
// path components on the way and/or expanding drive names. Returns NULL if the path name could not be expanded.
// The caller must deallocate the returned path using free().
char* tos_path_to_unix(char* tos_path);

// Wrapper around tos_path_to_unix that takes an emulated memory location instead of a char*
// As with tos_path_to_unix, the caller must deallocate the returned path using free().
char* read_path(emuptr32_t tos_path);
