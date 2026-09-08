/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/time.h>

/// @brief Sets file modification time (access time is ignored on VIMIX).
/// @param path File/dir which time to set.
/// @param times New modification time. Or NULL to set to current time.
/// @return 0 on success, -1 on error. Sets errno.
int utime(const char *path, const struct utimbuf *times);
