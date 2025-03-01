/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>

/// @brief Set the memory map
/// @param init_parameters Need the memory start address
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t plic_init(struct Device_Init_Parameters *init_parameters,
                const char *name);

/// @brief Enables an interrupt if priority > 0.
/// @param irq The IRQ to enable.
/// @param priority 0 disables the interrupt. Lower values have higher priority.
void plic_set_interrupt_priority(uint32_t irq, uint32_t priority);

/// @brief The index of the context to set s mode interrupt enable bits etc.
/// @return -1 if the hart has no s mode context (unusable for VIMUX)
ssize_t plic_get_hart_s_context(size_t hart_id);

/// @brief Called once per CPU core. Assumes that all devices that require
/// interrupts have already been created (otherwise no interrupts would get
/// enabled for them).
void plic_init_per_cpu();

/// ask the PLIC what interrupt we should serve.
int32_t plic_claim();

/// tell the PLIC we've served this IRQ.
void plic_complete(int32_t irq);
