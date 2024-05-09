# SPDX-License-Identifier: MIT

# Interrupts and exceptions while in supervisor
# mode (used for kernel mode) come here.
#
# The current stack is a kernel stack.
# Push all registers, call kernel_mode_interrupt_handler().
# When kernel_mode_interrupt_handler() returns, restore registers, return.

# C interrupt vector
.globl kernel_mode_interrupt_handler

#include "registers.h"

#if (__riscv_xlen == 32)
#define SIZE_OF_REG 4
#define SAVE_REG_TO_STACK(REGISTER, OFFSET) sw REGISTER, (OFFSET*SIZE_OF_REG)(sp)
#define LOAD_REG_FROM_STACK(REGISTER, OFFSET) lw REGISTER, (OFFSET*SIZE_OF_REG)(sp)
#else 
# 64 bit
#define SIZE_OF_REG 8
#define SAVE_REG_TO_STACK(REGISTER, OFFSET) sd REGISTER, (OFFSET*SIZE_OF_REG)(sp)
#define LOAD_REG_FROM_STACK(REGISTER, OFFSET) ld REGISTER, (OFFSET*SIZE_OF_REG)(sp)
#endif

.globl s_mode_trap_vector
.align 4
s_mode_trap_vector:
    # make room to save registers.
    addi sp, sp, -32*SIZE_OF_REG

    # save the registers.
    SAVE_REG_TO_STACK(ra,  IDX_RA )
    SAVE_REG_TO_STACK(sp,  IDX_SP )
    SAVE_REG_TO_STACK(gp,  IDX_GP )
    SAVE_REG_TO_STACK(tp,  IDX_TP )
    SAVE_REG_TO_STACK(t0,  IDX_T0 )
    SAVE_REG_TO_STACK(t1,  IDX_T1 )
    SAVE_REG_TO_STACK(t2,  IDX_T2 )
    SAVE_REG_TO_STACK(s0,  IDX_S0 )
    SAVE_REG_TO_STACK(s1,  IDX_S1 )
    SAVE_REG_TO_STACK(a0,  IDX_A0 )
    SAVE_REG_TO_STACK(a1,  IDX_A1 )
    SAVE_REG_TO_STACK(a2,  IDX_A2 )
    SAVE_REG_TO_STACK(a3,  IDX_A3 )
    SAVE_REG_TO_STACK(a4,  IDX_A4 )
    SAVE_REG_TO_STACK(a5,  IDX_A5 )
    SAVE_REG_TO_STACK(a6,  IDX_A6 )
    SAVE_REG_TO_STACK(a7,  IDX_A7 )
    SAVE_REG_TO_STACK(s2,  IDX_S2 )
    SAVE_REG_TO_STACK(s3,  IDX_S3 )
    SAVE_REG_TO_STACK(s4,  IDX_S4 )
    SAVE_REG_TO_STACK(s5,  IDX_S5 )
    SAVE_REG_TO_STACK(s6,  IDX_S6 )
    SAVE_REG_TO_STACK(s7,  IDX_S7 )
    SAVE_REG_TO_STACK(s8,  IDX_S8 )
    SAVE_REG_TO_STACK(s9,  IDX_S9 )
    SAVE_REG_TO_STACK(s10, IDX_S10)
    SAVE_REG_TO_STACK(s11, IDX_S11)
    SAVE_REG_TO_STACK(t3,  IDX_T3 )
    SAVE_REG_TO_STACK(t4,  IDX_T4 )
    SAVE_REG_TO_STACK(t5,  IDX_T5 )
    SAVE_REG_TO_STACK(t6,  IDX_T6 )

    # call the C interrupt handler in trap.c with the stack pointer as the parameter
    mv a0, sp
    call kernel_mode_interrupt_handler

    # restore registers.
    LOAD_REG_FROM_STACK(ra,  IDX_RA)
    LOAD_REG_FROM_STACK(sp,  IDX_SP)
    LOAD_REG_FROM_STACK(gp,  IDX_GP)
    # not tp (contains hartid), in case we moved CPUs
    LOAD_REG_FROM_STACK(t0,  IDX_T0 )
    LOAD_REG_FROM_STACK(t1,  IDX_T1 )
    LOAD_REG_FROM_STACK(t2,  IDX_T2 )
    LOAD_REG_FROM_STACK(s0,  IDX_S0 )
    LOAD_REG_FROM_STACK(s1,  IDX_S1 )
    LOAD_REG_FROM_STACK(a0,  IDX_A0 )
    LOAD_REG_FROM_STACK(a1,  IDX_A1 )
    LOAD_REG_FROM_STACK(a2,  IDX_A2 )
    LOAD_REG_FROM_STACK(a3,  IDX_A3 )
    LOAD_REG_FROM_STACK(a4,  IDX_A4 )
    LOAD_REG_FROM_STACK(a5,  IDX_A5 )
    LOAD_REG_FROM_STACK(a6,  IDX_A6 )
    LOAD_REG_FROM_STACK(a7,  IDX_A7 )
    LOAD_REG_FROM_STACK(s2,  IDX_S2 )
    LOAD_REG_FROM_STACK(s3,  IDX_S3 )
    LOAD_REG_FROM_STACK(s4,  IDX_S4 )
    LOAD_REG_FROM_STACK(s5,  IDX_S5 )
    LOAD_REG_FROM_STACK(s6,  IDX_S6 )
    LOAD_REG_FROM_STACK(s7,  IDX_S7 )
    LOAD_REG_FROM_STACK(s8,  IDX_S8 )
    LOAD_REG_FROM_STACK(s9,  IDX_S9 )
    LOAD_REG_FROM_STACK(s10, IDX_S10)
    LOAD_REG_FROM_STACK(s11, IDX_S11)
    LOAD_REG_FROM_STACK(t3,  IDX_T3 )
    LOAD_REG_FROM_STACK(t4,  IDX_T4 )
    LOAD_REG_FROM_STACK(t5,  IDX_T5 )
    LOAD_REG_FROM_STACK(t6,  IDX_T6 )

    addi sp, sp, 32*SIZE_OF_REG

    # return to whatever we were doing in the kernel.
    sret

