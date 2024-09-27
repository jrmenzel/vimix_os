/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

/// value from qemu, should be read from the device tree
#define timebase_frequency 10000000
#define TIMER_INTERRUPTS_PER_SECOND 100

dev_t clint_init(struct Device_Memory_Map *mapping);

#ifndef __ENABLE_SBI__
/// arrange to receive timer interrupts.
/// they will arrive in machine mode
/// at m_mode_trap_vector in m_mode_trap_vector.S,
/// which turns them into software interrupts for
/// interrupt_handler() in trap.c.
void clint_init_timer_interrupt();
#endif  // __ENABLE_SBI__

uint64_t rv_get_time();
