/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>

static inline uint64_t get_time() { return arm_read_cntpct_el0(); }

static inline uint64_t get_timebase_frequency(const void *)
{
    return arm_read_cntfrq_el0();
}
