/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/sbi.h>
#include <kernel/kernel.h>

static inline void arch_ipi_init() {}

static inline void arch_ipi_send_interrupt(uint64_t mask)
{
    sbi_send_ipi(mask);
}
