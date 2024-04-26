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
    int hart = smp_processor_id();

    // set enable bits for this hart's S-mode
    // for the uart and virtio disk.
    *(uint32_t*)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

    // set this hart's S-mode priority threshold to 0.
    *(uint32_t*)PLIC_SPRIORITY(hart) = 0;
}

/// ask the PLIC what interrupt we should serve.
int plic_claim()
{
    int hart = smp_processor_id();
    int irq = *(uint32_t*)PLIC_SCLAIM(hart);
    return irq;
}

/// tell the PLIC we've served this IRQ.
void plic_complete(int irq)
{
    int hart = smp_processor_id();
    *(uint32_t*)PLIC_SCLAIM(hart) = irq;
}
