/* SPDX-License-Identifier: MIT */

#pragma once

#include "clint.h"
#include "plic.h"
#include "sbi.h"

/// init once per CPU after one CPU called init_interrupt_controller()
static inline void init_interrupt_controller_per_hart() { plic_init_per_cpu(); }

/// @brief Enables an interrupt if priority > 0.
/// @param irq The IRQ to enable.
/// @param priority 0 disables the interrupt.
static inline void interrupt_controller_set_interrupt_priority(
    uint32_t irq, uint32_t priority)
{
    plic_set_interrupt_priority(irq, priority);
}
