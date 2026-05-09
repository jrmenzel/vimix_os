# SPDX-License-Identifier: MIT

#include <arch/riscv/asm/registers.h>

# Context switch
#
#   void context_switch(struct context *old, struct context *new);
# 
# Save current registers in old (a0). Load from new (a1).
#
# One context is a scheduler context (kernel stack the CPU booted on, kernel page table)
# The other is a kernel context of a user process (per process kernel stack, kernel page table).

#define SAVE_REG_TO_OLD(REGISTER, OFFSET) SAVE_REG_TO_ARRAY(REGISTER, OFFSET, a0)
#define LOAD_REG_FROM_NEW(REGISTER, OFFSET) LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, a1)

.globl context_switch
context_switch:
    SAVE_REG_TO_OLD(ra, IDX_CONTEXT_RA)
    SAVE_REG_TO_OLD(sp, IDX_CONTEXT_SP)
    SAVE_REG_TO_OLD(s0, IDX_CONTEXT_S0)
    SAVE_REG_TO_OLD(s1, IDX_CONTEXT_S1)
    SAVE_REG_TO_OLD(s2, IDX_CONTEXT_S2)
    SAVE_REG_TO_OLD(s3, IDX_CONTEXT_S3)
    SAVE_REG_TO_OLD(s4, IDX_CONTEXT_S4)
    SAVE_REG_TO_OLD(s5, IDX_CONTEXT_S5)
    SAVE_REG_TO_OLD(s6, IDX_CONTEXT_S6)
    SAVE_REG_TO_OLD(s7, IDX_CONTEXT_S7)
    SAVE_REG_TO_OLD(s8, IDX_CONTEXT_S8)
    SAVE_REG_TO_OLD(s9, IDX_CONTEXT_S9)
    SAVE_REG_TO_OLD(s10, IDX_CONTEXT_S10)
    SAVE_REG_TO_OLD(s11, IDX_CONTEXT_S11)

    LOAD_REG_FROM_NEW(ra, IDX_CONTEXT_RA)
    LOAD_REG_FROM_NEW(sp, IDX_CONTEXT_SP)
    LOAD_REG_FROM_NEW(s0, IDX_CONTEXT_S0)
    LOAD_REG_FROM_NEW(s1, IDX_CONTEXT_S1)
    LOAD_REG_FROM_NEW(s2, IDX_CONTEXT_S2)
    LOAD_REG_FROM_NEW(s3, IDX_CONTEXT_S3)
    LOAD_REG_FROM_NEW(s4, IDX_CONTEXT_S4)
    LOAD_REG_FROM_NEW(s5, IDX_CONTEXT_S5)
    LOAD_REG_FROM_NEW(s6, IDX_CONTEXT_S6)
    LOAD_REG_FROM_NEW(s7, IDX_CONTEXT_S7)
    LOAD_REG_FROM_NEW(s8, IDX_CONTEXT_S8)
    LOAD_REG_FROM_NEW(s9, IDX_CONTEXT_S9)
    LOAD_REG_FROM_NEW(s10, IDX_CONTEXT_S10)
    LOAD_REG_FROM_NEW(s11, IDX_CONTEXT_S11)
    ret

