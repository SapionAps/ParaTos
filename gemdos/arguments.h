#pragma once
#include "common.h"

int prepare_guest_args(char*** out_argv, char*** out_env, char* path, emuptr32_t cmdline, emuptr32_t env, size_t env_bytes);
