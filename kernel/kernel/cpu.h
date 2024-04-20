/* SPDX-License-Identifier: MIT */
#pragma once

// also include additional architecture-specific functions:
#include <arch/cpu.h>

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.
void push_off(void);
void pop_off(void);
