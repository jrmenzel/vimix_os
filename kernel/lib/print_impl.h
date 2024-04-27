/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/stdarg.h>
#include <kernel/types.h>

/// defines a function which can write one char:
typedef void (*_PUT_CHAR_FP)(const int32_t c, size_t payload);

/// define function which can read a char:
/// returns EOF of failure
typedef int32_t (*_GET_CHAR_FP)();

int32_t print_impl(_PUT_CHAR_FP func, size_t payload, const char* format,
                   va_list vl);
