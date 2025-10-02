/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

/// @brief Read an integer value from a SysFS path.
/// @param path SysFS path to read from.
/// @return File content interpreted as an int.
size_t get_from_sysfs(const char *path);

/// @brief Write an integer value to a SysFS path.
/// @param path SysFS path to write to.
/// @param value Value to write.
/// @return true on success, false on failure.
bool set_sysfs(const char *path, size_t value);
