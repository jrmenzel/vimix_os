/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
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
    size_t kernel_page_table;  // kernel page table
    size_t kernel_sp;          // top of process's kernel stack
    size_t kernel_trap;        // user_mode_interrupt_handler()
    size_t epc;                // saved user program counter
    size_t kernel_hartid;      // saved kernel tp
    size_t ra;                 // first register to save, index 5
    size_t sp;
    size_t gp;
    size_t tp;
    size_t t0;
    size_t t1;
    size_t t2;
    size_t s0;
    size_t s1;
    size_t a0;
    size_t a1;
    size_t a2;
    size_t a3;
    size_t a4;
    size_t a5;
    size_t a6;
    size_t a7;
    size_t s2;
    size_t s3;
    size_t s4;
    size_t s5;
    size_t s6;
    size_t s7;
    size_t s8;
    size_t s9;
    size_t s10;
    size_t s11;
    size_t t3;
    size_t t4;
    size_t t5;
    size_t t6;
};

// register_index 0 = register a0 etc
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

static inline void trapframe_set_return_register(struct trapframe *frame,
                                                 ssize_t value)
{
    frame->a0 = (xlen_t)value;
}

static inline xlen_t trapframe_get_return_register(struct trapframe *frame)
{
    return frame->a0;
}

static inline size_t trapframe_get_sys_call_number(struct trapframe *frame)
{
    // by ABI definition is the syscall number in a7 - just like on Linux :-)
    return frame->a7;
}

static inline xlen_t trapframe_get_frame_pointer(struct trapframe *frame)
{
    return frame->s0;
}

static inline xlen_t trapframe_get_return_address(struct trapframe *frame)
{
    return frame->ra;
}

/// @brief Prints the processes register state
/// @param frame trapframe of a not running process.
void debug_print_process_registers(struct trapframe *frame);
