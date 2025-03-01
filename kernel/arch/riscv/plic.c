/* SPDX-License-Identifier: MIT */

#include "plic.h"

#include <drivers/device.h>
#include <kernel/kernel.h>
#include <kernel/smp.h>

#include <mm/memlayout.h>

//
// the riscv Platform Level Interrupt Controller (PLIC).
//
// It supports 1023 interrupts (as int 0 is reserved).
// Each has a 32 bit priority and a pending bit.

/// 32 bit priority per interrupt, int 0 is reserved -> 1023 IRQs
#define PLIC_PRIORITY (g_plic.mmio_base + 0x00)

/// 32x 32 bit ints encoding 1024 interrupt pending bits
#define PLIC_PENDING (g_plic.mmio_base + 0x1000)

//
// Interrupts can be organized in Contexts, each enabling their own
// IRQ subset. One Context per CPU Hart and privilege mode is common.
//

/// address of one 32 bit int encoding 32 interrupt enable bits
#define PLIC_ENABLE(context, block) \
    (g_plic.mmio_base + 0x2000 + (context) * 0x80 + (block) * 4)

/// if set prio of context is > prio of interrupt, then it is ignored
#define PLIC_PRIORITY_THRESHOLD(context) \
    (g_plic.mmio_base + 0x200000 + (context) * 0x1000)

/// address contains IRQ number of latext interrupt of context
/// write IRQ back to clear IRQ.
#define PLIC_CLAIM(context) (g_plic.mmio_base + 0x200004 + (context) * 0x1000)

struct
{
    size_t mmio_base;
    bool plic_is_initialized;
} g_plic = {0xc000000, false};

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

dev_t plic_init(struct Device_Init_Parameters *init_parameters,
                const char *name)
{
    if (g_plic.plic_is_initialized)
    {
        return 0;
    }
    g_plic.mmio_base = init_parameters->mem[0].start;
    g_plic.plic_is_initialized = true;
    return MKDEV(PLIC_MAJOR, 0);
}

void plic_set_interrupt_priority(uint32_t irq, uint32_t priority)
{
    *(uint32_t *)(g_plic.mmio_base + (irq * sizeof(uint32_t))) = priority;
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

/// @brief The index of the context to set s mode interrupt enable bits etc.
/// @return
size_t plic_get_this_harts_s_context()
{
    size_t hart = smp_processor_id();
#if defined(__PLATFORM_VISIONFIVE2)
    // first context is reserved for the S7 core which has no S mode (and VIMIX
    // wont boot there)
    return 1 + (2 * hart + 1);
#else
    // per hart: M mode context, S mode context...
    return 2 * hart + 1;
#endif
}

void plic_init_per_cpu()
{
    uint32_t irq_enable_flags[ENABLE_BLOCKS];
    for (size_t block = 0; block < ENABLE_BLOCKS; ++block)
    {
        irq_enable_flags[block] = 0;
    }
    for (size_t i = 0; i < MAX_DEVICES * MAX_MINOR_DEVICES; ++i)
    {
        struct Device *dev = g_devices[i];
        if (dev && (dev->irq_number != INVALID_IRQ_NUMBER))
        {
            size_t block = dev->irq_number / 32;
            size_t enable_bit = dev->irq_number % 32;
            irq_enable_flags[block] |= (1 << enable_bit);
        }
    }

    size_t context = plic_get_this_harts_s_context();
    plic_enable_interrupts(context, irq_enable_flags);
}

int32_t plic_claim()
{
    size_t context = plic_get_this_harts_s_context();
    int32_t irq = *(uint32_t *)PLIC_CLAIM(context);
    return irq;
}

void plic_complete(int32_t irq)
{
    size_t context = plic_get_this_harts_s_context();
    *(uint32_t *)PLIC_CLAIM(context) = irq;
}
