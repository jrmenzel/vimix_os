/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void plic_init();
void plic_init_per_cpu();

/// ask the PLIC what interrupt we should serve.
int32_t plic_claim();

/// tell the PLIC we've served this IRQ.
void plic_complete(int32_t irq);
