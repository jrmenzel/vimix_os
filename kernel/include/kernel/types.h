/* SPDX-License-Identifier: MIT */
#pragma once

// This header can get included from:
// - kernel and userspace apps: they want to get the std defines from this file
// - development PC apps like mkfs: they should include the platforms
//   C std includes to avoid redefinition errors.
//   -> set __USE_REAL_STDC for apps on real OSes which need this include
#ifdef __USE_REAL_STDC
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
#define INVALID_DEVICE (0)

/// @brief UNIX file descriptor, must be int as this is dictated by the public
/// UNIX/C API (stdio.h et al) Exposed to user space (e.g. open()). Internally
/// an index into the per-process file list in struct process->files.
typedef int FILE_DESCRIPTOR;

/// @brief Inode mode (e.g. for mknod).
/// Encodes type (file, device, etc) as well as
/// access rights (rxwrxwrxw), see stat.h.
typedef uint32_t mode_t;

/// @brief User ID, negative values mean invalid
typedef int32_t uid_t;

/// @brief Group ID, negative values mean invalid
typedef int32_t gid_t;

/// @brief Byte offset inside of a file.
typedef ssize_t off_t;

/// @brief Clock ID for clock_gettime().
typedef int32_t clockid_t;

/// @brief clockid_t for system-wide realtime clock where time 0 is 1.1.1970.
#define CLOCK_REALTIME 0

/// @brief clockid_t for a monotonic system-wide clock where time 0 is
/// undefined.
#define CLOCK_MONOTONIC 1

#endif  // __USE_REAL_STDC

#define INVALID_FILE_DESCRIPTOR (-1)

/// @brief Inode number, 32 bit on 32 bit systems, 64 bit on 64 bit systems.
typedef unsigned long ino_t;
#define INVALID_INODE (0)

/// @brief Process ID.
typedef int32_t pid_t;

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif
