/* SPDX-License-Identifier: MIT */

#include <arch/arm64/arm64.h>
#include <arch/arm64/drivers/gic_v2.h>
#include <arch/interrupts.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>

void cpu_set_boot_state() {}

void ipi_init_per_cpu()
{
    // Ensure the dedicated IPI SGI is enabled on each CPU interface.
    gic2_set_interrupt_priority(ARM64_IPI_SGI_ID, 1);
}

void dtb_get_cpu_features(const void *dtb, size_t cpu_id,
                          CPU_Features *features_out)
{
    size_t ctr = arm_read_ctr_el0();

    size_t dminline = (size_t)((ctr >> 16) & 0x0f);
    features_out->dcache_line_size_bytes = (size_t)4 << dminline;

    size_t iminline = (size_t)(ctr & 0x0f);
    features_out->icache_line_size_bytes = (size_t)4 << iminline;

    features_out->idc_flag = ((ctr >> 28) & 0x1) != 0;
    features_out->dic_flag = ((ctr >> 29) & 0x1) != 0;
}
