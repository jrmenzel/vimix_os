# SPDX-License-Identifier: MIT

# Interrupts and exceptions while in supervisor
# mode (used for kernel mode) come here.
#
# The current stack is a kernel stack.
# Push all registers, call kernel_mode_interrupt_handler().
# When kernel_mode_interrupt_handler() returns, restore registers, return.

# C interrupt vector
.globl kernel_mode_interrupt_handler

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
    SAVE_REG_TO_STACK(ra,   0)
    SAVE_REG_TO_STACK(sp,   1)
    SAVE_REG_TO_STACK(gp,   2)
    SAVE_REG_TO_STACK(tp,   3)
    SAVE_REG_TO_STACK(t0,   4)
    SAVE_REG_TO_STACK(t1,   5)
    SAVE_REG_TO_STACK(t2,   6)
    SAVE_REG_TO_STACK(s0,   7)
    SAVE_REG_TO_STACK(s1,   8)
    SAVE_REG_TO_STACK(a0,   9)
    SAVE_REG_TO_STACK(a1,  10)
    SAVE_REG_TO_STACK(a2,  11)
    SAVE_REG_TO_STACK(a3,  12)
    SAVE_REG_TO_STACK(a4,  13)
    SAVE_REG_TO_STACK(a5,  14)
    SAVE_REG_TO_STACK(a6,  15)
    SAVE_REG_TO_STACK(a7,  16)
    SAVE_REG_TO_STACK(s2,  17)
    SAVE_REG_TO_STACK(s3,  18)
    SAVE_REG_TO_STACK(s4,  19)
    SAVE_REG_TO_STACK(s5,  20)
    SAVE_REG_TO_STACK(s6,  21)
    SAVE_REG_TO_STACK(s7,  22)
    SAVE_REG_TO_STACK(s8,  23)
    SAVE_REG_TO_STACK(s9,  24)
    SAVE_REG_TO_STACK(s10, 25)
    SAVE_REG_TO_STACK(s11, 26)
    SAVE_REG_TO_STACK(t3,  27)
    SAVE_REG_TO_STACK(t4,  28)
    SAVE_REG_TO_STACK(t5,  29)
    SAVE_REG_TO_STACK(t6,  30)

    # call the C interrupt handler in trap.c
    call kernel_mode_interrupt_handler

    # restore registers.
    LOAD_REG_FROM_STACK(ra,   0)
    LOAD_REG_FROM_STACK(sp,   1)
    LOAD_REG_FROM_STACK(gp,   2)
    # not tp (contains hartid), in case we moved CPUs
    LOAD_REG_FROM_STACK(t0,   4)
    LOAD_REG_FROM_STACK(t1,   5)
    LOAD_REG_FROM_STACK(t2,   6)
    LOAD_REG_FROM_STACK(s0,   7)
    LOAD_REG_FROM_STACK(s1,   8)
    LOAD_REG_FROM_STACK(a0,   9)
    LOAD_REG_FROM_STACK(a1,  10)
    LOAD_REG_FROM_STACK(a2,  11)
    LOAD_REG_FROM_STACK(a3,  12)
    LOAD_REG_FROM_STACK(a4,  13)
    LOAD_REG_FROM_STACK(a5,  14)
    LOAD_REG_FROM_STACK(a6,  15)
    LOAD_REG_FROM_STACK(a7,  16)
    LOAD_REG_FROM_STACK(s2,  17)
    LOAD_REG_FROM_STACK(s3,  18)
    LOAD_REG_FROM_STACK(s4,  19)
    LOAD_REG_FROM_STACK(s5,  20)
    LOAD_REG_FROM_STACK(s6,  21)
    LOAD_REG_FROM_STACK(s7,  22)
    LOAD_REG_FROM_STACK(s8,  23)
    LOAD_REG_FROM_STACK(s9,  24)
    LOAD_REG_FROM_STACK(s10, 25)
    LOAD_REG_FROM_STACK(s11, 26)
    LOAD_REG_FROM_STACK(t3,  27)
    LOAD_REG_FROM_STACK(t4,  28)
    LOAD_REG_FROM_STACK(t5,  29)
    LOAD_REG_FROM_STACK(t6,  30)

    addi sp, sp, 32*SIZE_OF_REG

    # return to whatever we were doing in the kernel.
    sret

