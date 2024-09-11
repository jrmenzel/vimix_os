/* SPDX-License-Identifier: MIT */

#include "plic.h"

#include <drivers/device.h>
#include <kernel/kernel.h>
#include <kernel/smp.h>
#include <mm/memlayout.h>

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void plic_set_interrupt_priority(uint32_t irq, uint32_t priority)
{
    *(uint32_t *)(PLIC_BASE + (irq * sizeof(uint32_t))) = priority;
}

const size_t ENABLE_BLOCKS = 32;  ///< # of int32 -> 1024 bits total

void plic_enable_interrupts(size_t context,
                            uint32_t irq_enable_flags[ENABLE_BLOCKS])
{
    // Step 1: Set the enable bit
    for (size_t block = 0; block < ENABLE_BLOCKS; ++block)
    {
        *(uint32_t *)PLIC_ENABLE(context, block) = irq_enable_flags[block];
    }

    // Step 2: set the priority threshold to 0,
    // enabling all interrupts with a higher priority (and set enable bit)
    *(uint32_t *)PLIC_PRIORITY_THRESHOLD(context) = 0;
}

size_t plic_get_this_harts_context()
{
    // per hart: M mode context, S mode context...
    size_t hart = smp_processor_id();
    return 2 * hart + 1;
}

void plic_init_per_cpu()
{
    uint32_t irq_enable_flags[ENABLE_BLOCKS];
    for (size_t block = 0; block < ENABLE_BLOCKS; ++block)
    {
        irq_enable_flags[block] = 0;
    }
    for (size_t i = 0; i < MAX_DEVICES; ++i)
    {
        struct Device *dev = g_devices[i];
        if (dev && (dev->irq_number != INVALID_IRQ_NUMBER))
        {
            size_t block = dev->irq_number / 32;
            size_t enable_bit = dev->irq_number % 32;
            irq_enable_flags[block] |= (1 << enable_bit);
        }
    }

    size_t context = plic_get_this_harts_context();
    plic_enable_interrupts(context, irq_enable_flags);
}

int32_t plic_claim()
{
    size_t context = plic_get_this_harts_context();
    int32_t irq = *(uint32_t *)PLIC_CLAIM(context);
    return irq;
}

void plic_complete(int32_t irq)
{
    size_t context = plic_get_this_harts_context();
    *(uint32_t *)PLIC_CLAIM(context) = irq;
}
