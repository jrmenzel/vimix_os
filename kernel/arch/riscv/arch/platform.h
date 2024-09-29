/* SPDX-License-Identifier: MIT */
#pragma once

#include <sbi.h>

#ifdef __ENABLE_SBI__
static inline void init_platform() { init_sbi(); }
#else
static inline void init_platform() {}
#endif
