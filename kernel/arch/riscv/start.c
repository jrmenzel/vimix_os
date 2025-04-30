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

void cpu_set_boot_state() { rv_write_csr_sie(0); }
