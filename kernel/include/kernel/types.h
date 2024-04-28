/* SPDX-License-Identifier: MIT */
#pragma once

// This header can get included from:
// - kernel and userspace apps: they want to get the std defines from this file
// - development PC apps like mkfs: they should include the platforms
//   C std includes to avoid redefinition errors.
//   -> set __USE_REAL_STDC__ for apps on real OSes which need this include
#ifdef __USE_REAL_STDC__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <kernel/stdbool.h>
#include <kernel/stddef.h>
#include <kernel/stdint.h>

/// @brief Encodes major and minor device numbers.
/// Don't make any assumptions about the bits reserved for
/// MAJOR vs MINOR number or the size of a full dev_t. Use the MKDEV macro.
typedef int32_t dev_t;

/// @brief UNIX file descriptor, must be int as this is dictated by the public
/// UNIX/C API (stdio.h et al) Exposed to user space (e.g. open()). Internally
/// an index into the per-process file list in struct task_struct->files.
typedef int FILE_DESCRIPTOR;

#endif  // __USE_REAL_STDC__

typedef unsigned char uchar;
#define INVALID_FILE_DESCRIPTOR (-1)

/// @brief Process ID.
typedef int32_t pid_t;

typedef uint64_t pde_t;
