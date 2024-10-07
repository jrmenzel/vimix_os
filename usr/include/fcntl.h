/* SPDX-License-Identifier: MIT */
#pragma once

// File Control Operations
// (FCoNTroLl...)
//

#include <kernel/fcntl.h>

#include <stdint.h>
#include <stdio.h>

/// @brief opens a file
/// @param pathname filename
/// @param flags access / create flags
/// @param mode optional file mode, set this when creating a file
/// @return the file descriptor as an int or -1 on failure
extern int open(const char *pathname, int32_t flags, ... /*mode_t mode*/);

/// @brief "A call to creat() is equivalent to calling open() with flags equal
/// to O_CREAT|O_WRONLY|O_TRUNC." - Linux manpage open(2)
static inline int creat(const char *pathname, mode_t mode)
{
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}
