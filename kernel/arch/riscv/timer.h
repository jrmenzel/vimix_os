/* SPDX-License-Identifier: MIT */

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

/// value from CLIT/qemu, should be read from the device tree
#define timebase_frequency 10000000
#define TIMER_INTERRUPTS_PER_SECOND 100

typedef void(timer_schedule_interrupt_p)(uint64_t time);

/// @brief Set in timer_init() and should be called from
/// handle_timer_interrupt()
extern timer_schedule_interrupt_p *timer_schedule_interrupt;

/// @brief Called from start() and sets timer_schedule_interrupt() pointer
/// depending on the configured timer.
void timer_init();