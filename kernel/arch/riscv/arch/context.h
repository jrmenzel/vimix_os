/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

/// Saved registers for kernel context switches.
struct context
{
    xlen_t ra;
    xlen_t sp;

    // callee-saved
    xlen_t s0;
    xlen_t s1;
    xlen_t s2;
    xlen_t s3;
    xlen_t s4;
    xlen_t s5;
    xlen_t s6;
    xlen_t s7;
    xlen_t s8;
    xlen_t s9;
    xlen_t s10;
    xlen_t s11;
};

/// @brief Stores the current register values and restores the ones from
/// src_of_register. As the ra register holds the functions return address,
/// and the sp register holds its stack pointer, the function return in
/// context_switch will return to the thread of execution that was stored in
/// src_of_register. (implementation: context_switch.S)
/// @param dst_for_registers Where the current registers are stored to.
/// @param src_of_register Where the new register values are loaded from.
void context_switch(struct context *dst_for_registers,
                    struct context *src_of_register);

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
                                                 size_t value)
{
    frame->a0 = value;
}

static inline size_t trapframe_get_sys_call_number(struct trapframe *frame)
{
    // by ABI definition is the syscall number in a7 - just like on Linux :-)
    return frame->a7;
}

// register_index 0 = register a0 etc
size_t trapframe_get_argument_register(struct trapframe *frame,
                                       size_t register_index);
