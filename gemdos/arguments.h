#pragma once
#include "common.h"

void prepare_guest_args(char*** out_argv, char*** out_env, char* path, emuptr32_t cmdline, emuptr32_t env);
