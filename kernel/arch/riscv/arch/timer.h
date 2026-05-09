/* SPDX-License-Identifier: MIT */

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

static inline uint64_t get_time() { return rv_get_time(); }
