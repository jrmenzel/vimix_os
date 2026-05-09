/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

typedef int32_t (*interrupt_claim_func_p)();
typedef void (*interrupt_complete_func_p)(int32_t irq);

/// @brief Enables an interrupt if priority > 0.
/// @param irq The IRQ to enable.
/// @param priority 0 disables the interrupt.
typedef void (*set_interrupt_priority_func_p)(uint32_t irq, uint32_t priority);

typedef void (*interrupt_init_per_cpu_func_p)();

struct Interrupt_Controller_Interface
{
    interrupt_claim_func_p claim;
    interrupt_complete_func_p complete;
    set_interrupt_priority_func_p set_priority;
    interrupt_init_per_cpu_func_p init_per_cpu;
};

extern struct Interrupt_Controller_Interface g_int_con;
