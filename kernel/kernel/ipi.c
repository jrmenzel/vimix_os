/* SPDX-License-Identifier: MIT */

#include <kernel/ipi.h>
#include <kernel/proc.h>

void ipi_init() { spin_lock_init(&g_cpus_ipi_lock, "ipi_lock"); }

cpu_mask ipi_cpu_mask_all()
{
    cpu_mask mask = 0;
    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        if (g_cpus[i].state != CPU_UNUSED)
        {
            mask |= (1 << i);
        }
    }
    return mask;
}
