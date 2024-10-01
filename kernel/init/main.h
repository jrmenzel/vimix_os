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

//
// Some const values about the kernel binary provided by the linker.
//

/// @brief First address after the kernel.
extern char start_of_kernel[];

/// @brief End of kernel binary (not data)
extern char end_of_kernel[];

/// @brief Start of (expected to be) zero initialized data
extern char bss_start[];

/// @brief End of BSS section
extern char bss_end[];

/// @brief Size of kernel binary in bytes
extern char size_of_text[];

/// @brief Size of RO kernel data in bytes
extern char size_of_rodata[];

/// @brief Size of RW kernel data in size
extern char size_of_data[];

/// @brief Size of BSS section in bytes
extern char size_of_bss[];
