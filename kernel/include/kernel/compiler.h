/* SPDX-License-Identifier: MIT */
#pragma once

#ifdef __clang__

#define diagnostic_push _Pragma("clang diagnostic push");
#define diagnostic_pop _Pragma("clang diagnostic pop");

// ignore most GCC specific warnings
#define diagnostic_ignore_fd_leak
#define diagnostic_ignore_fd_use_without_check
#define diagnostic_ignore_infinite_recursion
#define diagnostic_ignore_malloc_leak

#define diagnostic_ignore_format_security \
    _Pragma("clang diagnostic ignored \"-Wformat-security\"");

#define diagnostic_infinite_recursion
#define diagnostic_fd_access_mode_mismatch
#define diagnostic_fd_use_without_check
#define diagnostic_null_dereference

#elif defined(__GNUC__)

#define diagnostic_push _Pragma("GCC diagnostic push");
#define diagnostic_pop _Pragma("GCC diagnostic pop");

// GCC static analyzer hints
#define diagnostic_ignore_fd_leak \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-fd-leak\"");

#define diagnostic_ignore_fd_use_without_check \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-fd-use-without-check\"");

#define diagnostic_ignore_infinite_recursion \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-infinite-recursion\"");

#define diagnostic_ignore_malloc_leak \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-malloc-leak\"");

#define diagnostic_ignore_format_security \
    _Pragma("GCC diagnostic ignored \"-Wformat-security\"");

#define diagnostic_infinite_recursion \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-infinite-recursion\"");

#define diagnostic_fd_access_mode_mismatch \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-fd-access-mode-mismatch\"");

#define diagnostic_fd_use_without_check \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-fd-use-without-check\"");

#define diagnostic_null_dereference \
    _Pragma("GCC diagnostic ignored \"-Wanalyzer-null-dereference\"");

#else
#error "Unknown compiler"
#endif
