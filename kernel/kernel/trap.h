/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

//
// generic functions for all architectures
//

/// Interrupts and exceptions go here via trap vectors
/// on whatever the current kernel stack is.
void kernel_mode_interrupt_handler(size_t *stack);

void user_mode_interrupt_handler(size_t *stack);

//
// the following functions are implemented per ARCH:
//

/// return to user space after a syscall
void return_to_user_mode();

void handle_timer_interrupt();

void handle_device_interrupt();

/// @brief Handle an Inter Processor Interrupt
/// @return true if the current process (if there is one) should yield
bool handle_ipi_interrupt();

/// @brief Dump kernel thread state from before the interrupt.
void dump_pre_int_kthread_state(size_t *stack);

struct Interrupt_Context;

/// @brief Prints the scause name and other scause specific info
void dump_exception_cause(struct Interrupt_Context *ctx);
