/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

extern struct debug_info *g_kernel_debug_info;

syserr_t panic_load_debug_symbols(const char *debug_file_path);

/// @brief Prints the kernel call stack pointed to by frame_pointer
/// @param frame_pointer Frame pointer.
void debug_print_call_stack_kernel_fp(size_t frame_pointer);

void debug_print_pc(size_t pc);

void debug_print_ra(size_t ra);
