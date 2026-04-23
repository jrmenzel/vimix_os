/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/stdarg.h>
#include <kernel/types.h>

#if defined(__ARCH_32BIT)
#define FORMAT_REG_SIZE "0x%08zx"
#else
#define FORMAT_REG_SIZE "0x%016zx"
#endif

// call at boot as soon as possible
void printk_init();

// call after a console device was initialized
void printk_redirect_to_console();

// called on panic to prevent deadlocks from printing the last messages
void printk_disable_locking();

// like printf() but instead of printing chars via sys calls the kernel directly
// handles the output
void printk(char *format, ...) __attribute__((format(printf, 1, 2)));

int vsnprintf(char *dst, size_t n, const char *format, va_list arg)
    __attribute__((format(printf, 3, 0)));

int snprintf(char *dst, size_t n, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/// @brief Kernel panic: print an error and halt the OS or shutdown
void panic(char *error_message) __attribute__((noreturn));
