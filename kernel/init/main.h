/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// all values above 0 so the initialized global variable will
// *not* be in BSS (cleared by the kernel after reading this var)
#define GLOBAL_INIT_NOT_STARTED 1
#define GLOBAL_INIT_BSS_CLEAR 2
#define GLOBAL_INIT_DONE 3
extern volatile size_t g_global_init_done;

///
/// first function called in supervisor mode for each hart
void main(void *device_tree, size_t is_first_thread);
