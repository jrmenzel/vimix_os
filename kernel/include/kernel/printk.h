/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

// call at boot as soon as possible
void printk_init();

// like printf() but instead of printing chars via sys calls the kernel directly
// handles the output
void printk(char *format, ...) __attribute__((format(printf, 1, 2)));

/// @brief Kernel panic: print an error and halt the OS
#ifdef DEBUG_AUTOSTART_USERTESTS
void panic(char *error_message);
#else
void panic(char *error_message) __attribute__((noreturn));
#endif

// set to true after a kernel panic
extern volatile size_t g_kernel_panicked;
