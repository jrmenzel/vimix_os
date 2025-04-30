/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <sbi.h>

static inline void init_platform() { init_sbi(); }
static inline void platform_boot_other_cpus(void *dtb)
{
    sbi_start_harts((size_t)dtb);
}
