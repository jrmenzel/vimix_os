/* SPDX-License-Identifier: MIT */
#pragma once

//
// GICv2 interrupt controller driver. This is used on older ARMv8 platforms,
// and the only supported interrupt controller for ARM.
//

#include <drivers/driver_list.h>
#include <kernel/kernel.h>

dev_t gic2_init(struct Device_Init_Parameters *init_parameters,
                const char *name);

void gic2_set_interrupt_priority(uint32_t irq, uint32_t priority);

/// Send a Software Generated Interrupt to a CPU target list bitmask.
/// Bit 0 targets CPU interface 0, bit 1 targets CPU interface 1, etc.
void gic2_send_sgi(uint32_t sgi_id, uint8_t target_list);

void gic2_init_global();

void gic2_init_per_cpu();

/// ask the GICv2 what interrupt we should serve.
int32_t gic2_claim();

/// ask the GICv2 what interrupt is pending without acknowledging it.
int32_t gic2_peek_pending();

/// tell the GICv2 we've served this IRQ.
void gic2_complete(int32_t irq);

void ipi_init_per_cpu();
