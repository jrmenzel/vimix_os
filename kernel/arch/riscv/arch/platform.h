/* SPDX-License-Identifier: MIT */
#pragma once

#include <sbi.h>

#ifdef __ENABLE_SBI__
static inline void init_platform(void *dtb) { init_sbi(dtb); }
#else
static inline void init_platform(void *dtb) {}
#endif
