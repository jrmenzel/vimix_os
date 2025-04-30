# SPDX-License-Identifier: MIT

# Interrupts and exceptions while in supervisor
# mode (used for kernel mode) come here.
#
# The current stack is a kernel stack.
# Push all registers, call kernel_mode_interrupt_handler().
# When kernel_mode_interrupt_handler() returns, restore registers, return.

# C interrupt vector
.globl kernel_mode_interrupt_handler

#include <arch/riscv/asm/registers.h>

#define SAVE_REG_TO_STACK(REGISTER, OFFSET) SAVE_REG_TO_ARRAY(REGISTER, OFFSET, sp)
#define LOAD_REG_FROM_STACK(REGISTER, OFFSET) LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, sp)

.globl s_mode_trap_vector
# the address of this function must be 4 byte aligned because the lowest 
# 2 bits of the stvec register are reserved for a mode.
.align 4
s_mode_trap_vector:
    # make room to save registers.
    addi sp, sp, -REG_COUNT_TO_SAVE*SIZE_OF_REG

    # save the registers.
    SAVE_REG_TO_STACK(ra,  IDX_RA )
    SAVE_REG_TO_STACK(gp,  IDX_GP )
    SAVE_REG_TO_STACK(tp,  IDX_TP )
    SAVE_REG_TO_STACK(t0,  IDX_T0 )
    SAVE_REG_TO_STACK(t1,  IDX_T1 )
    SAVE_REG_TO_STACK(t2,  IDX_T2 )
    SAVE_REG_TO_STACK(a0,  IDX_A0 )
    SAVE_REG_TO_STACK(a1,  IDX_A1 )
    SAVE_REG_TO_STACK(a2,  IDX_A2 )
    SAVE_REG_TO_STACK(a3,  IDX_A3 )
    SAVE_REG_TO_STACK(a4,  IDX_A4 )
    SAVE_REG_TO_STACK(a5,  IDX_A5 )
    SAVE_REG_TO_STACK(a6,  IDX_A6 )
    SAVE_REG_TO_STACK(a7,  IDX_A7 )
    SAVE_REG_TO_STACK(t3,  IDX_T3 )
    SAVE_REG_TO_STACK(t4,  IDX_T4 )
    SAVE_REG_TO_STACK(t5,  IDX_T5 )
    SAVE_REG_TO_STACK(t6,  IDX_T6 )

    # call the C interrupt handler in trap.c with the stack pointer as the parameter
    # note that s0-s11 and sp are saved/restored by the called function 
    mv a0, sp
    call kernel_mode_interrupt_handler

    # restore registers.
    LOAD_REG_FROM_STACK(ra,  IDX_RA)
    LOAD_REG_FROM_STACK(gp,  IDX_GP)
    # not tp (contains hartid), in case we moved CPUs
    LOAD_REG_FROM_STACK(t0,  IDX_T0 )
    LOAD_REG_FROM_STACK(t1,  IDX_T1 )
    LOAD_REG_FROM_STACK(t2,  IDX_T2 )
    LOAD_REG_FROM_STACK(a0,  IDX_A0 )
    LOAD_REG_FROM_STACK(a1,  IDX_A1 )
    LOAD_REG_FROM_STACK(a2,  IDX_A2 )
    LOAD_REG_FROM_STACK(a3,  IDX_A3 )
    LOAD_REG_FROM_STACK(a4,  IDX_A4 )
    LOAD_REG_FROM_STACK(a5,  IDX_A5 )
    LOAD_REG_FROM_STACK(a6,  IDX_A6 )
    LOAD_REG_FROM_STACK(a7,  IDX_A7 )
    LOAD_REG_FROM_STACK(t3,  IDX_T3 )
    LOAD_REG_FROM_STACK(t4,  IDX_T4 )
    LOAD_REG_FROM_STACK(t5,  IDX_T5 )
    LOAD_REG_FROM_STACK(t6,  IDX_T6 )

    addi sp, sp, REG_COUNT_TO_SAVE*SIZE_OF_REG

    # return to whatever we were doing in the kernel.
    sret

