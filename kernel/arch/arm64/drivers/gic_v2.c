/* SPDX-License-Identifier: MIT */

#include <arch/arm64/drivers/gic_v2.h>
#include <arch/arm64/drivers/gic_v2_defs.h>
#include <arch/interrupts.h>
#include <arch/irq.h>
#include <drivers/device.h>
#include <drivers/driver.h>
#include <kernel/interrupt_controller.h>
#include <kernel/kobject.h>
#include <kernel/param.h>
#include <kernel/smp.h>

REGISTER_DRIVER("arm,gic-400", gic2_init);
REGISTER_DRIVER("arm,cortex-a15-gic", gic2_init);

struct gic2
{
    size_t mmio_base_cpu;
    size_t mmio_base_dist;
};

struct gic2 g_gic2 = {0};

// Per-CPU last acknowledged IAR token. SGI completion on GICv2 should use
// the original token, not only the 10-bit interrupt ID.
static uint32_t g_gic2_last_ack_token[MAX_CPUS] = {0};

dev_t gic2_init(struct Device_Init_Parameters *init_parameters,
                const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);

    // Device tree GICv2 reg order is distributor first, then CPU interface.
    g_gic2.mmio_base_dist = init_parameters->mem[0].start_va;
    g_gic2.mmio_base_cpu = init_parameters->mem[1].start_va;

    g_int_con.claim = gic2_claim;
    g_int_con.complete = gic2_complete;
    g_int_con.set_priority = gic2_set_interrupt_priority;
    g_int_con.init_per_cpu = gic2_init_per_cpu;

    return MKDEV(GIC2_MAJOR, 0);
}

static void gic2_enable_int(uint32_t irq)
{
    // GICD_ISENABLER is write-1-to-set; avoid RMW.
    MMIO_WRITE_UINT_32(g_gic2.mmio_base_dist, GICD_ISENABLER(irq / 32),
                       (1u << (irq % 32)));
}

static void gic2_disable_int(uint32_t irq)
{
    // GICD_ICENABLER is write-1-to-clear; avoid RMW.
    MMIO_WRITE_UINT_32(g_gic2.mmio_base_dist, GICD_ICENABLER(irq / 32),
                       (1u << (irq % 32)));
}

static void gic2_clear_pending(uint32_t irq)
{
    // GICD_ICPENDR is write-1-to-clear; avoid RMW.
    MMIO_WRITE_UINT_32(g_gic2.mmio_base_dist, GICD_ICPENDR(irq / 32),
                       (1u << (irq % 32)));
}

static void gic2_set_prio0(uint32_t irq)
{
    // set priority to 0
    uint32_t val = (uint32_t)0xff << (irq % 4 * 8);

    MMIO_CLEAR_BITS_UINT_32(g_gic2.mmio_base_dist, GICD_IPRIORITYR(irq / 4),
                            val);
}

void gic2_set_target(uint32_t irq, uint32_t cpuid)
{
    uint32_t reg =
        MMIO_READ_UINT_32(g_gic2.mmio_base_dist, GICD_ITARGETSR(irq / 4));
    reg &= ~((uint32_t)0xff << (irq % 4 * 8));
    reg |= (uint32_t)(1 << cpuid) << (irq % 4 * 8);
    MMIO_WRITE_UINT_32(g_gic2.mmio_base_dist, GICD_ITARGETSR(irq / 4), reg);
}

void gic2_set_interrupt_priority(uint32_t irq, uint32_t priority)
{
    if (priority > 0)
    {
        // SPIs (>=32) require an explicit target CPU in GICv2.
        if (irq >= 32)
        {
            gic2_set_target(irq, (uint32_t)smp_processor_id());
        }

        // enable with highest priority (0 on GICv2)
        gic2_clear_pending(irq);
        gic2_enable_int(irq);
        gic2_set_prio0(irq);
    }
    else
    {
        // priority 0 means "disable"
        gic2_clear_pending(irq);
        gic2_disable_int(irq);
    }
}

void gic2_init_global()
{
    // Some KVM-backed GICv2 setups fault on EL1 writes to GICD_IGROUPR
    // (0x80..). Avoid the global group sweep and only enable forwarding.
    MMIO_WRITE_UINT_32(g_gic2.mmio_base_dist, GICD_CTLR, 0x3);
}

void gic2_send_sgi(uint32_t sgi_id, uint8_t target_list)
{
    // Ensure queued IPI data is globally visible before raising the SGI.
    atomic_thread_fence(memory_order_seq_cst);

    if (sgi_id >= 16)
    {
        printk("GICv2: invalid SGI id %u\n", sgi_id);
        return;
    }

    if (target_list == 0)
    {
        return;
    }

    if (g_gic2.mmio_base_dist == 0)
    {
        printk("GICv2: distributor not initialized, SGI dropped\n");
        return;
    }

    // CPUTargetListFilter=0 (use CPUTargetList), SGIINTID in bits [3:0].
    uint32_t sgir = (sgi_id & 0x0f) | ((uint32_t)target_list << 16);
    MMIO_WRITE_UINT_32(g_gic2.mmio_base_dist, GICD_SGIR, sgir);
}

void gic2_init_per_cpu()
{
    // enable IRQs of all registered devices:
    rwspin_read_lock(&g_kobjects_dev.children_lock);
    struct list_head *pos;
    list_for_each(pos, &g_kobjects_dev.children)
    {
        struct kobject *kobj = kobject_from_child_list(pos);
        struct Device *dev = device_from_kobj(kobj);
        if (dev->irq_number != INVALID_IRQ_NUMBER)
        {
            gic2_set_prio0(dev->irq_number);
        }
    }
    rwspin_read_unlock(&g_kobjects_dev.children_lock);

    // The architected generic timer is not a registered MMIO device, so
    // enable its PPI explicitly on each CPU.
    gic2_clear_pending(ARM64_TIMER_IRQ_VIRTUAL);
    gic2_enable_int(ARM64_TIMER_IRQ_VIRTUAL);
    gic2_set_prio0(ARM64_TIMER_IRQ_VIRTUAL);

    MMIO_WRITE_UINT_32(g_gic2.mmio_base_cpu, GICC_CTLR,
                       0x3);  // enable Group0/1 handling, FIQ disabled
    MMIO_WRITE_UINT_32(g_gic2.mmio_base_cpu, GICC_PMR,
                       0xff);  // set priority mask to allow all priorities
}

int32_t gic2_claim()
{
    const uint32_t GICC_IAR_MASK = 0x3ff;  // lower 10 bits are the interrupt ID
    uint32_t iar = MMIO_READ_UINT_32(g_gic2.mmio_base_cpu, GICC_IAR);
    uint32_t irq = iar & GICC_IAR_MASK;

    size_t cpu_id = smp_processor_id();
    if (cpu_id < MAX_CPUS)
    {
        g_gic2_last_ack_token[cpu_id] = iar;
    }

    // 1020..1023 are special IDs, not dispatchable device/IPI IRQs.
    if (irq >= 1020)
    {
        return INVALID_IRQ_NUMBER;
    }
    return irq;
}

int32_t gic2_peek_pending()
{
    uint32_t hppir = MMIO_READ_UINT_32(g_gic2.mmio_base_cpu, GICC_HPPIR);
    uint32_t irq = hppir & 0x3ff;  // lower 10 bits are the interrupt ID
    // 1020..1023 are special IDs, not dispatchable device/IPI IRQs.
    if (irq >= 1020)
    {
        return INVALID_IRQ_NUMBER;
    }
    return irq;
}

/// tell the GICv2 we've served this IRQ.
void gic2_complete(int32_t irq)
{
    uint32_t eoir = (uint32_t)irq;
    size_t cpu_id = smp_processor_id();
    if (cpu_id < MAX_CPUS)
    {
        uint32_t ack = g_gic2_last_ack_token[cpu_id];
        if ((ack & 0x3ffu) == (uint32_t)irq)
        {
            eoir = ack;
        }
        g_gic2_last_ack_token[cpu_id] = 0;
    }

    MMIO_WRITE_UINT_32(g_gic2.mmio_base_cpu, GICC_EOIR, eoir);
}
