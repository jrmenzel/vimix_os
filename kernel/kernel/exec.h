/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

/// @brief Implements syscall execv.
/// @param pathname Name of ELF file to execute
/// @param argv Command arguments
/// @return -1 on failure
int32_t execv(char *pathname, char **argv);
