/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

extern uint64_t g_timebase_frequency;
#define TIMER_INTERRUPTS_PER_SECOND 100

typedef void(timer_schedule_interrupt_p)(uint64_t time);

/// @brief Set in timer_init() and should be called from
/// handle_timer_interrupt()
extern timer_schedule_interrupt_p *timer_schedule_interrupt;

/// @brief Called from start() and sets timer_schedule_interrupt() pointer
/// depending on the configured timer.
void timer_init(void *dtb, CPU_Features features);

static inline uint64_t get_time() { return rv_get_time(); }
