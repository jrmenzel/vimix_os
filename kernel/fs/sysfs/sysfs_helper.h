/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief Converts a string parameter for *_ops_store functions to an integer.
/// @param buf The buffer containing the string.
/// @param n String length.
/// @param ok Returns if a number was successfully parsed.
/// @return Parsed integer value, 0 if no number could be parsed.
int32_t store_param_to_int(const char *buf, size_t n, bool *ok);
