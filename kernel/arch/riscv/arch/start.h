/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// Pick thread for all non-parallel init stuff:
// - Based on global flag for SBI (no race condition thanks to how harts are
// started in SBI mode)
// - Based on CPU ID otherwise
#ifdef CONFIG_RISCV_SBI
#define is_first_thread(cpuid) (g_global_init_done == GLOBAL_INIT_NOT_STARTED)
#else
#define is_first_thread(cpuid) (cpuid == 0)
#endif  // CONFIG_RISCV_SBI

#if defined(CONFIG_RISCV_BOOT_M_MODE)
void jump_to_main_in_s_mode(void *dtb, xlen_t is_first);
#define jump_to_main jump_to_main_in_s_mode
#else
#define jump_to_main main
#endif
