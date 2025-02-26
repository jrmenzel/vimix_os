/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

#if defined(_arch_is_32bit)
#define FORMAT_REG_SIZE "0x%08zx"
#else
#define FORMAT_REG_SIZE "0x%016zx"
#endif

// call at boot as soon as possible
void printk_init();

// like printf() but instead of printing chars via sys calls the kernel directly
// handles the output
void printk(char *format, ...) __attribute__((format(printf, 1, 2)));

int snprintf(char *dst, size_t n, const char *fmt, ...);

/// @brief Kernel panic: print an error and halt the OS or shutdown
void panic(char *error_message);

// set to true after a kernel panic
extern volatile size_t g_kernel_panicked;
