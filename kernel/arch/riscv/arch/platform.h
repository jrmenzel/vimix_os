/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <sbi.h>

#ifdef CONFIG_RISCV_SBI
static inline void init_platform(void *dtb) { init_sbi(dtb); }
#else
static inline void init_platform(void *dtb) {}
#endif
