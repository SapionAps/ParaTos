#pragma once
#include <stdint.h>
#include <time.h>

#include "tos_errors.h"

extern char* paratos_exe;
void FindExecutable(void);
void InitM68KMemory(void);
int32_t MapErrno(void);
uint32_t unixtime2dos(const time_t* unixtime);
time_t dostime2unix(uint32_t dostime);
