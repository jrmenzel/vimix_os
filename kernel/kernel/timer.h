/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/timer.h>
#include <kernel/kernel.h>

typedef void(timer_schedule_interrupt_p)(uint64_t time, uint64_t interval);

extern uint64_t g_timebase_frequency;
#define TIMER_INTERRUPTS_PER_SECOND 100

extern uint64_t g_boot_time;

/// @brief Set in timer_init() and should be called from
/// handle_timer_interrupt()
extern timer_schedule_interrupt_p *timer_schedule_interrupt;

/// @brief Called from start() and sets timer_schedule_interrupt() pointer
/// depending on the configured timer.
void timer_init(const void *dtb, CPU_Features features);

/// @brief To be implemented by architecture
/// @param features CPU features
/// @return Pointer to the timer schedule interrupt function
timer_schedule_interrupt_p *arch_timer_interrupt_func(CPU_Features features);
