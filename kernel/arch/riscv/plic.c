/* SPDX-License-Identifier: MIT */

#include <arch/riscv/plic.h>
#include <drivers/device.h>
#include <drivers/mmio_access.h>
#include <init/dtb.h>
#include <kernel/kernel.h>
#include <kernel/smp.h>
#include <libfdt.h>
#include <mm/memlayout.h>

//
// the riscv Platform Level Interrupt Controller (PLIC).
//
// It supports 1023 interrupts (as int 0 is reserved).
// Each has a 32 bit priority and a pending bit.
// 32 bit priority per interrupt, int 0 is reserved -> 1023 IRQs
// 32x 32 bit ints encoding 1024 interrupt pending bits

//
// Interrupts can be organized in Contexts, each enabling their own
// IRQ subset. One Context per CPU Hart and privilege mode is common.
//

/// address of one 32 bit int encoding 32 interrupt enable bits
#define PLIC_ENABLE_REG_OFFSET(context, block) \
    (0x2000 + (context) * 0x80 + (block) * 4)

/// if set prio of context is > prio of interrupt, then it is ignored
#define PLIC_PRIORITY_THRESHOLD_REG_OFFSET(context) \
    (0x200000 + (context) * 0x1000)

struct
{
    size_t mmio_base;
    bool plic_is_initialized;
    ssize_t hart_context[MAX_CPUS];
} g_plic = {0, false};

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

bool get_hart_int_controller_phandles(void *dtb,
                                      uint32_t *hart_int_controller_phandle,
                                      size_t max_handles)
{
    memset(hart_int_controller_phandle, 0, max_handles * sizeof(uint32_t));

    for (size_t cpu = 0; cpu < max_handles; ++cpu)
    {
        int offset = dtb_get_cpu_offset(dtb, cpu, false);
        if (offset < 0) break;

        int controller_offset =
            fdt_subnode_offset(dtb, offset, "interrupt-controller");
        if (controller_offset < 0)
        {
            printk("dtb error: interrupt-controller not defined for CPU %zd\n",
                   cpu);
            return false;
        }
        const uint32_t *phandle_dtb =
            fdt_getprop(dtb, controller_offset, "phandle", NULL);
        if (phandle_dtb == NULL)
        {
            printk(
                "dtb error: interrupt-controller for CPU %zd has no phandle\n",
                cpu);
            return false;
        }
        uint32_t phandle = fdt32_to_cpu(phandle_dtb[0]);

        hart_int_controller_phandle[cpu] = phandle;
    }

    return true;
}

#define MAX_CONTEXTS (MAX_CPUS * 2)
void plic_init_hart_context_lookup(void *dtb, size_t plic_offset)
{
    // unless read differently from the device tree:
    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        // [context m mode cpu 0][context s mode cpu 0][context m mode cpu
        // 1]... Only the S Mode contexts are relevant:
        g_plic.hart_context[i] = 2 * i + 1;
    }

    // invalid dtb
    if (dtb == NULL && plic_offset == 0) return;

    int int_ext_len;
    const uint32_t *int_ext =
        fdt_getprop(dtb, plic_offset, "interrupts-extended", &int_ext_len);
    if (int_ext == NULL)
    {
        printk(
            "PLIC dtb error, interrupts-extended not found, assuming "
            "defaults\n");
        return;
    }

    // number of bytes to number of values
    int_ext_len /= sizeof(uint32_t);

    // interrupts-extended contains the harts of the PLIC contexts
    // Some systems have additional managemant cores, which are not
    // available to VIMIX from S Mode. In this case the hart to context map
    // needs to be derrived from this dtb property.
    // Each hart available for VIMIX should have two entries: M Mode and S
    // Mode. only the S Mode entry index is relevant.

    // index is the context, value is a reference to the hart
    size_t hart_for_context[MAX_CONTEXTS];
    for (size_t i = 0; i < MAX_CONTEXTS; ++i)
    {
        // for every context the hart reference and some adittional data is
        // stored
        size_t idx = i * 2;
        if (idx >= int_ext_len)
        {
            hart_for_context[i] = (-1);
            continue;
        }
        hart_for_context[i] = fdt32_to_cpu(int_ext[idx]);
    }

    // now hart_for_context[i] is a phandle to cpu@X/interrupt-controller

    // the system can have more CPUs as VIMIX can use
    uint32_t hart_int_controller_phandle[MAX_CPUS];
    if (get_hart_int_controller_phandles(dtb, hart_int_controller_phandle,
                                         MAX_CPUS) == false)
    {
        return;
    }

    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        g_plic.hart_context[i] = -1;

        if (hart_int_controller_phandle[i] != 0)
        {
            bool m_mode_ctx_found = false;
            for (size_t ctx = 0; ctx < MAX_CONTEXTS; ++ctx)
            {
                if (hart_for_context[ctx] == hart_int_controller_phandle[i])
                {
                    // the s mode context is the second one
                    if (m_mode_ctx_found)
                    {
                        g_plic.hart_context[i] = ctx;
                        break;
                    }
                    m_mode_ctx_found = true;
                }
            }
        }
    }
}

dev_t plic_init(struct Device_Init_Parameters *init_parameters,
                const char *name)
{
    if (g_plic.plic_is_initialized)
    {
        return 0;
    }
    g_plic.mmio_base = init_parameters->mem[0].start;

    plic_init_hart_context_lookup(init_parameters->dtb,
                                  init_parameters->dev_offset);

    g_plic.plic_is_initialized = true;
    return MKDEV(PLIC_MAJOR, 0);
}

void plic_set_interrupt_priority(uint32_t irq, uint32_t priority)
{
    // if the PLIC is not initialized (e.g. the boot console tries to
    // register itself) ignore this call. plic_init_per_cpu() will also set
    // default priorities for all registered devices. In those cases the
    // requested priority is lost, that's a ToDo for when priorities are
    // used.
    if (g_plic.plic_is_initialized)
    {
        // register offset is irq * sizeof(uint32_t) = irq << 2
        MMIO_WRITE_UINT_32_SHIFT(g_plic.mmio_base, irq, 2, priority);
    }
}

uint32_t plic_get_interrupt_priority(uint32_t irq)
{
    if (g_plic.plic_is_initialized)
    {
        // register offset is irq * sizeof(uint32_t) = irq << 2
        return MMIO_READ_UINT_32_SHIFT(g_plic.mmio_base, irq, 2);
    }
    else
    {
        return 0;
    }
}

const size_t ENABLE_BLOCKS = 32;  ///< # of int32 -> 1024 bits total

void plic_enable_interrupts(size_t context,
                            uint32_t irq_enable_flags[ENABLE_BLOCKS])
{
    // Step 1: Set the enable bit
    for (size_t block = 0; block < ENABLE_BLOCKS; ++block)
    {
        size_t reg_offset = PLIC_ENABLE_REG_OFFSET(context, block);
        MMIO_WRITE_UINT_32(g_plic.mmio_base, reg_offset,
                           irq_enable_flags[block]);
    }

    // Step 2: set the priority threshold to 0,
    // enabling all interrupts with a higher priority (and set enable bit)
    size_t reg_offset = PLIC_PRIORITY_THRESHOLD_REG_OFFSET(context);
    MMIO_WRITE_UINT_32(g_plic.mmio_base, reg_offset, 0);
}

ssize_t plic_get_hart_s_context(size_t hart_id)
{
    return g_plic.hart_context[hart_id];
}

static inline ssize_t plic_get_this_harts_s_context()
{
    return plic_get_hart_s_context(smp_processor_id());
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

            uint32_t irq_prio = plic_get_interrupt_priority(dev->irq_number);
            if (irq_prio == 0)
            {
                // set the default priority in case the device did not call
                // plic_set_interrupt_priority() before (or the prio was
                // lost as the PLIC was not initialized yes)
                plic_set_interrupt_priority(dev->irq_number, 1);
            }
        }
    }

    size_t context = plic_get_this_harts_s_context();
    plic_enable_interrupts(context, irq_enable_flags);
}

int32_t plic_claim()
{
    size_t context = plic_get_this_harts_s_context();
    int32_t irq =
        MMIO_READ_UINT_32(g_plic.mmio_base, 0x200004 + (context) * 0x1000);
    return irq;
}

void plic_complete(int32_t irq)
{
    size_t context = plic_get_this_harts_s_context();
    // write IRQ back to clear IRQ.
    MMIO_WRITE_UINT_32(g_plic.mmio_base, 0x200004 + (context) * 0x1000, irq);
}
