/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <mm/memlayout.h>

//
// the riscv Platform Level Interrupt Controller (PLIC).
//
// It supports 1023 interrupts (as int 0 is reserved).
// Each has a 32 bit priority and a pending bit.

/// 32 bit priority per interrupt, int 0 is reserved -> 1023 IRQs
#define PLIC_PRIORITY (PLIC_BASE + 0x00)

/// 32x 32 bit ints encoding 1024 interrupt pending bits
#define PLIC_PENDING (PLIC_BASE + 0x1000)

//
// Interrupts can be organized in Contexts, each enabling their own
// IRQ subset. One Context per CPU Hart and privilege mode is common.
//

/// address of one 32 bit int encoding 32 interrupt enable bits
#define PLIC_ENABLE(context, block) \
    (PLIC_BASE + 0x2000 + (context) * 0x80 + (block) * 4)

/// if set prio of context is > prio of interrupt, then it is ignored
#define PLIC_PRIORITY_THRESHOLD(context) \
    (PLIC_BASE + 0x200000 + (context) * 0x1000)

/// address contains IRQ number of latext interrupt of context
/// write IRQ back to clear IRQ.
#define PLIC_CLAIM(context) (PLIC_BASE + 0x200004 + (context) * 0x1000)

/// Size of the memory map starting at PLIC_BASE
#define PLIC_SIZE 0x04000000

/// @brief Enables an interrupt if priority > 0.
/// @param irq The IRQ to enable.
/// @param priority 0 disables the interrupt.
void plic_set_interrupt_priority(uint32_t irq, uint32_t priority);

/// @brief Called once per CPU core. Assumes that all devices that require
/// interrupts have already been created (otherwise no interrupts would get
/// enabled for them).
void plic_init_per_cpu();

/// ask the PLIC what interrupt we should serve.
int32_t plic_claim();

/// tell the PLIC we've served this IRQ.
void plic_complete(int32_t irq);
