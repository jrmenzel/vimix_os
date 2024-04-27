/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

// call at boot as soon as possible
void printk_init();

// like printf() but instead of printing chars via sys calls the kernel directly
// handles the output
void printk(char *format, ...);

/// @brief Kernel panic: print an error and halt the OS
void panic(char *error_message) __attribute__((noreturn));
