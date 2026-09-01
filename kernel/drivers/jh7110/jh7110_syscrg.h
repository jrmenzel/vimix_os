/* SPDX-License-Identifier: MIT */
#pragma once

// Driver for the starfive,jh7110-syscrg clock controller.
// Used by the jh7110_temp sensor driver.

#include <drivers/devices_list.h>
#include <kernel/kernel.h>

/// JH7110 clocks
/// Values from
/// https://doc-en.rvspace.org/JH7110/TRM/JH7110_TRM/sys_crg.html#sys_crg__section_v3t_pfn_wsb
#define SYSCLK_TEMP_APB 129   // Bus
#define SYSCLK_TEMP_CORE 130  // Sense

/// JH7110 resets
#define RSTN_BASE 190  // End of the clock list
// to assert or deassert a reset, write a 1 or 0 to the corresponding bit
#define RSTN_TEMP_APB 123   // Bus
#define RSTN_TEMP_CORE 124  // Sense

dev_t jh7110_syscrg_init(struct Device_Init_Parameters *init_parameters,
                         const char *name);

void jh7110_syscrg_enable(int num_clk);
void jh7110_syscrg_deassert(int num_rst);
