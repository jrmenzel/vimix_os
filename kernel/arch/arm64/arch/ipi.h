/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/arm64/drivers/gic_v2.h>
#include <arch/interrupts.h>
#include <kernel/kernel.h>

static inline void arch_ipi_init() { gic2_init_global(); }

static inline void arch_ipi_send_interrupt(uint64_t target_mask)
{
    gic2_send_sgi(ARM64_IPI_SGI_ID, (uint8_t)target_mask);
}
