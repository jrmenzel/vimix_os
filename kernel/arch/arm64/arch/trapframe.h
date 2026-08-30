/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/arm64/arm64.h>
#include <kernel/kernel.h>

/// per-process data for the trap handling code in u_mode_trap_vector.S.
/// sits in a page by itself just under the trampoline page in the
/// user page table. not specially mapped in the kernel page table.
/// u_mode_trap_vector in u_mode_trap_vector.S saves user registers in the
/// trapframe, then initializes registers from the trapframe's kernel_sp,
/// kernel_hartid, kernel_page_table, and jumps to kernel_trap.
/// return_to_user_mode() and return_to_user_mode_asm in u_mode_trap_vector.S
/// set up the trapframe's kernel_*, restore user registers from the trapframe,
/// switch to the user page table, and enter user space. the trapframe includes
/// callee-saved user registers like s0-s11 because the return-to-user path via
/// return_to_user_mode() doesn't return through the entire kernel call stack.
struct trapframe
{
    size_t x0;
    size_t x1;
    size_t x2;
    size_t x3;
    size_t x4;
    size_t x5;
    size_t x6;
    size_t x7;
    size_t x8;
    size_t x9;
    size_t x10;
    size_t x11;
    size_t x12;
    size_t x13;
    size_t x14;
    size_t x15;
    size_t x16;
    size_t x17;
    size_t x18;
    size_t x19;
    size_t x20;
    size_t x21;
    size_t x22;
    size_t x23;
    size_t x24;
    size_t x25;
    size_t x26;
    size_t x27;
    size_t x28;
    size_t x29;
    size_t x30;
    size_t elr;
    size_t spsr;
    size_t sp;
    size_t
        kernel_page_table;  // kernel page table (register value, not pointer)
    size_t kernel_sp;       // top of process's kernel stack
    size_t kernel_trap;     // user_mode_interrupt_handler()
    size_t epc;             // saved user program counter
    size_t kernel_hartid;   // saved kernel tp
};

size_t trapframe_get_argument_register(struct trapframe *frame,
                                       size_t register_index);

void trapframe_set_argument_register(struct trapframe *frame,
                                     size_t register_index, size_t value);

static inline void trapframe_set_program_counter(struct trapframe *frame,
                                                 size_t pc)
{
    frame->epc = pc;
}

static inline size_t trapframe_get_program_counter(struct trapframe *frame)
{
    return frame->epc;
}

static inline void trapframe_set_stack_pointer(struct trapframe *frame,
                                               size_t sp)
{
    frame->sp = sp;
}

static inline size_t trapframe_get_stack_pointer(struct trapframe *frame)
{
    return frame->sp;
}

static inline void trapframe_set_return_register(struct trapframe *frame,
                                                 ssize_t value)
{
    frame->x0 = (size_t)value;
}

static inline size_t trapframe_get_return_register(struct trapframe *frame)
{
    return frame->x0;
}

static inline size_t trapframe_get_sys_call_number(struct trapframe *frame)
{
    // by ABI definition is the syscall number in x8 - just like on Linux :-)
    return frame->x8;
}

static inline size_t trapframe_get_frame_pointer(struct trapframe *frame)
{
    return frame->x29;
}

static inline size_t trapframe_get_return_address(struct trapframe *frame)
{
    return frame->x30;
}

/// @brief Prints the processes register state
/// @param frame trapframe of a not running process.
void debug_print_process_registers(struct trapframe *frame);
