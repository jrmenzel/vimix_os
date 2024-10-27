/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/string.h>

/// @brief Returns a pointer to a string describing the error code (from
/// errno.h)
/// @param errnum Error code (e.g. errno)
/// @return String describing the error. Do not edit.
char *strerror(int errnum);
