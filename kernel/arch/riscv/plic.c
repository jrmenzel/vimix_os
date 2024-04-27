/* SPDX-License-Identifier: MIT */

#include "plic.h"

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <mm/memlayout.h>

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void plic_init()
{
    // set desired IRQ priorities non-zero (otherwise disabled).
    *(uint32_t*)(PLIC + UART0_IRQ * 4) = 1;
    *(uint32_t*)(PLIC + VIRTIO0_IRQ * 4) = 1;
}

void plic_init_per_cpu()
{
    size_t hart = smp_processor_id();

    // set enable bits for this hart's S-mode
    // for the uart and virtio disk.
    *(uint32_t*)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

    // set this hart's S-mode priority threshold to 0.
    *(uint32_t*)PLIC_SPRIORITY(hart) = 0;
}

int32_t plic_claim()
{
    size_t hart = smp_processor_id();
    int32_t irq = *(uint32_t*)PLIC_SCLAIM(hart);
    return irq;
}

void plic_complete(int32_t irq)
{
    size_t hart = smp_processor_id();
    *(uint32_t*)PLIC_SCLAIM(hart) = irq;
}
