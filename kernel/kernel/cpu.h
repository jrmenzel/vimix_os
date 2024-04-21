/* SPDX-License-Identifier: MIT */
#pragma once

// also include additional architecture-specific functions:
#include <arch/cpu.h>

// cpu_push_disable_device_interrupt_stack/cpu_pop_disable_device_interrupt_stack
// are like cpu_disable_device_interrupts()/cpu_enable_device_interrupts()
// except that they are matched: it takes two
// cpu_pop_disable_device_interrupt_stack()s to undo two
// cpu_push_disable_device_interrupt_stack()s.  Also, if interrupts are
// initially off, then cpu_push_disable_device_interrupt_stack,
// cpu_pop_disable_device_interrupt_stack leaves them off.
void cpu_push_disable_device_interrupt_stack();
void cpu_pop_disable_device_interrupt_stack();
