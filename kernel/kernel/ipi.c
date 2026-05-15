/* SPDX-License-Identifier: MIT */

#include <kernel/ipi.h>
#include <kernel/proc.h>

void ipi_init()
{
    spin_lock_init(&g_cpus_ipi_lock, "ipi_lock");
    arch_ipi_init();
}

cpu_mask ipi_cpu_mask_all()
{
    cpu_mask mask = 0;
    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        if (g_cpus[i].state == CPU_STARTED)
        {
            mask |= (1 << i);
        }
    }
    return mask;
}

void ipi_send_interrupt(cpu_mask mask, enum ipi_type type, void *data)
{
    cpu_mask target_mask = 0;

    spin_lock(&g_cpus_ipi_lock);
    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        if ((mask & ((cpu_mask)1 << i)) == 0)
        {
            // don't call the CPU running this code.
            continue;
        }

        struct cpu *c = &g_cpus[i];

        // error check
        if (c->state == CPU_UNUSED)
        {
            printk("IPI: CPU %zd not started, cannot send IPI %d\n", i, type);
            continue;
        }

        // find pending IPI count
        size_t pending_count = 0;
        while ((pending_count < MAX_IPI_PENDING) &&
               (c->ipi[pending_count].pending != IPI_NONE))
        {
            pending_count++;
        }

        // if exactly the same IPI is already pending, don't add
        // another one
        if ((pending_count != 0) &&
            (c->ipi[pending_count - 1].pending == type) &&
            (c->ipi[pending_count - 1].data == data))
        {
            continue;
        }

        if (pending_count == MAX_IPI_PENDING)
        {
            printk("IPI queue full on CPU %zd, dropping IPI %d\n", i, type);
            continue;
        }

        c->ipi[pending_count].pending = type;
        c->ipi[pending_count].data = data;
        target_mask = target_mask | (1 << i);
    }
    spin_unlock(&g_cpus_ipi_lock);

    arch_ipi_send_interrupt(target_mask);
}
