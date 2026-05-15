/* SPDX-License-Identifier: MIT */

#include <arch/riscv/riscv.h>
#include <init/dtb.h>
#include <kernel/kernel.h>

static inline uint64_t get_time() { return rv_get_time(); }

static inline uint64_t get_timebase_frequency(const void *dtb)
{
    return dtb_get_timebase(dtb);
}
