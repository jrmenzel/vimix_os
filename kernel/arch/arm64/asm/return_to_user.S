/* SPDX-License-Identifier: MIT */

#include <arch/arm64/asm/arm64_def.h>
#include <arch/arm64/asm/registers.h>
#include <mm/memlayout.h>

#define SAVE_REG_TO_TRAP_FRAME(REGISTER, OFFSET) SAVE_REG_TO_ARRAY(REGISTER, OFFSET, x0)
#define LOAD_REG_FROM_TRAP_FRAME(REGISTER, OFFSET) LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, x0)

.equ ARM64_TRAPFRAME_VA, 0x3ffffff000

.section trampsec

.globl return_to_user_mode_switch
.type return_to_user_mode_switch, %function
return_to_user_mode_switch:
    // return_to_user_mode_switch(user_ttbr0, asid, flush_tlb, kernel_stack)
    // Commit to the user page table and immediately continue with the
    // register restore path. Do not return through a C frame after ttbr0 changes.
    msr ttbr0_el1, x0
    dsb ish
    isb
    cbz x2, 1f
    lsl x1, x1, #TTBR_ASID_POS
    tlbi aside1, x1
    dsb ish
    isb
1:
    mov x0, x3

.globl return_to_user_mode_asm
.type return_to_user_mode_asm, %function
return_to_user_mode_asm:

    // store kernel stack
    mov sp, x0

    movz x0, #(ARM64_TRAPFRAME_VA & 0xffff)
    movk x0, #((ARM64_TRAPFRAME_VA >> 16) & 0xffff), lsl #16
    movk x0, #((ARM64_TRAPFRAME_VA >> 32) & 0xffff), lsl #32

    // skip x0, which is the trapframe pointer
    LOAD_REG_FROM_TRAP_FRAME(x1, IDX_TRAPFRAME_X1)
    LOAD_REG_FROM_TRAP_FRAME(x2, IDX_TRAPFRAME_X2)
    LOAD_REG_FROM_TRAP_FRAME(x3, IDX_TRAPFRAME_X3)
    LOAD_REG_FROM_TRAP_FRAME(x4, IDX_TRAPFRAME_X4)
    LOAD_REG_FROM_TRAP_FRAME(x5, IDX_TRAPFRAME_X5)
    LOAD_REG_FROM_TRAP_FRAME(x6, IDX_TRAPFRAME_X6)
    LOAD_REG_FROM_TRAP_FRAME(x7, IDX_TRAPFRAME_X7)
    LOAD_REG_FROM_TRAP_FRAME(x8, IDX_TRAPFRAME_X8)
    LOAD_REG_FROM_TRAP_FRAME(x9, IDX_TRAPFRAME_X9)
    LOAD_REG_FROM_TRAP_FRAME(x10, IDX_TRAPFRAME_X10)
    LOAD_REG_FROM_TRAP_FRAME(x11, IDX_TRAPFRAME_X11)
    LOAD_REG_FROM_TRAP_FRAME(x12, IDX_TRAPFRAME_X12)
    LOAD_REG_FROM_TRAP_FRAME(x13, IDX_TRAPFRAME_X13)
    LOAD_REG_FROM_TRAP_FRAME(x14, IDX_TRAPFRAME_X14)
    LOAD_REG_FROM_TRAP_FRAME(x15, IDX_TRAPFRAME_X15)
    LOAD_REG_FROM_TRAP_FRAME(x16, IDX_TRAPFRAME_X16)
    LOAD_REG_FROM_TRAP_FRAME(x17, IDX_TRAPFRAME_X17)
    LOAD_REG_FROM_TRAP_FRAME(x18, IDX_TRAPFRAME_X18)
    LOAD_REG_FROM_TRAP_FRAME(x19, IDX_TRAPFRAME_X19)
    LOAD_REG_FROM_TRAP_FRAME(x20, IDX_TRAPFRAME_X20)
    LOAD_REG_FROM_TRAP_FRAME(x21, IDX_TRAPFRAME_X21)
    LOAD_REG_FROM_TRAP_FRAME(x22, IDX_TRAPFRAME_X22)
    LOAD_REG_FROM_TRAP_FRAME(x23, IDX_TRAPFRAME_X23)
    LOAD_REG_FROM_TRAP_FRAME(x24, IDX_TRAPFRAME_X24)
    LOAD_REG_FROM_TRAP_FRAME(x25, IDX_TRAPFRAME_X25)
    LOAD_REG_FROM_TRAP_FRAME(x26, IDX_TRAPFRAME_X26)
    LOAD_REG_FROM_TRAP_FRAME(x27, IDX_TRAPFRAME_X27)
    LOAD_REG_FROM_TRAP_FRAME(x28, IDX_TRAPFRAME_X28)
    LOAD_REG_FROM_TRAP_FRAME(x29, IDX_TRAPFRAME_X29)
    LOAD_REG_FROM_TRAP_FRAME(x30, IDX_TRAPFRAME_X30)

    LOAD_REG_FROM_TRAP_FRAME(x0, IDX_TRAPFRAME_X0)

    eret
