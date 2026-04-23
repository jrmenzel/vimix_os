/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <arch/timer.h>
#include <init/dtb.h>
#include <init/main.h>
#include <init/start.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/page.h>

void cpu_set_boot_state()
{
    rv_write_csr_sie(0);
    cpu_disable_interrupts();
}

// atomically read when performing the lottery to get the hart preparing the
// initial page table, so keep 32 bit
int32_t g_early_pt_hart = 1;

// signal that the early page table is ready when set to 0.
// starting at 1 to keep out of BSS.
int32_t g_early_pt_done = 1;
