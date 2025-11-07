/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

/// @brief Implements syscall execv.
/// @param pathname Name of ELF file to execute
/// @param argv Command arguments
/// @return -errno on failure
syserr_t do_execv(char *pathname, char **argv);
