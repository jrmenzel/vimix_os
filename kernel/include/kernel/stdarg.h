/* SPDX-License-Identifier: MIT */
#pragma once

#ifdef __USE_REAL_STDC
// This file can get included by apps compiled for the host (e.g. tools that
// need VIMIX includes). Those should use the host headers to avoid
// redefinitions.
#include <stdarg.h>
#else

#if __GNUC__
// Own defines for gcc if compiling the kernel or apps with own clib.
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v, l) __builtin_va_arg(v, l)

typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;

#else  // __GNUC__

// Dummie defines for syntax highlighting in VStudio
#pragma error "not intended to be compiled, add compiler support?"

typedef void *va_list;

#define va_start(v, l)
#define va_end(v)
#define va_arg(v, l) (*(l *)(v))

#endif  // __GNUC__

#endif  // __USE_REAL_STDC
