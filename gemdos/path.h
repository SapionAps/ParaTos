#pragma once
#define PATH_SEP(c) ((c)=='/' || (c)=='\\')
#define END_OF_NAME(c) ((c)==0 || PATH_SEP(c))

// Utility function that takes a source file name and converts it to a 8.3 TOS file name.
// Tos domain processes will also get the name converted to uppercase.
// The destination buffer has to be 13 bytes large.
const char* filename8_3(char* dest, const char* source);
