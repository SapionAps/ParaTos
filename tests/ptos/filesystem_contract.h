#pragma once
#include <stdint.h>

int filesystem_test_native(uint16_t fn, intptr_t a, intptr_t b, intptr_t c, intptr_t d, int32_t *result);
void test_filesystem(void);
