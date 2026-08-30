/* SPDX-License-Identifier: MIT */

#include <arch/arm64/asm/registers.h>

// Context switch
//
//   void context_switch(struct context *old, struct context *new);
// 
// Save current registers in old (x0). Load from new (x1).
//
// One context is a scheduler context (kernel stack the CPU booted on, kernel page table)
// The other is a kernel context of a user process (per process kernel stack, kernel page table).

#define SAVE_REG_TO_OLD(REGISTER, OFFSET) SAVE_REG_TO_ARRAY(REGISTER, OFFSET, x0)
#define LOAD_REG_FROM_NEW(REGISTER, OFFSET) LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, x1)

.globl context_switch
context_switch:
    #define SP_TMP x9
    mov     SP_TMP, sp
    SAVE_REG_TO_OLD(SP_TMP, IDX_CONTEXT_SP)
    SAVE_REG_TO_OLD(x18, IDX_CONTEXT_X18)
    SAVE_REG_TO_OLD(x19, IDX_CONTEXT_X19)
    SAVE_REG_TO_OLD(x20, IDX_CONTEXT_X20)
    SAVE_REG_TO_OLD(x21, IDX_CONTEXT_X21)
    SAVE_REG_TO_OLD(x22, IDX_CONTEXT_X22)
    SAVE_REG_TO_OLD(x23, IDX_CONTEXT_X23)
    SAVE_REG_TO_OLD(x24, IDX_CONTEXT_X24)
    SAVE_REG_TO_OLD(x25, IDX_CONTEXT_X25)
    SAVE_REG_TO_OLD(x26, IDX_CONTEXT_X26)
    SAVE_REG_TO_OLD(x27, IDX_CONTEXT_X27)
    SAVE_REG_TO_OLD(x28, IDX_CONTEXT_X28)
    SAVE_REG_TO_OLD(x29, IDX_CONTEXT_X29)
    SAVE_REG_TO_OLD(x30, IDX_CONTEXT_X30)

    LOAD_REG_FROM_NEW(SP_TMP, IDX_CONTEXT_SP)
    mov     sp, SP_TMP
    LOAD_REG_FROM_NEW(x18, IDX_CONTEXT_X18)
    LOAD_REG_FROM_NEW(x19, IDX_CONTEXT_X19)
    LOAD_REG_FROM_NEW(x20, IDX_CONTEXT_X20)
    LOAD_REG_FROM_NEW(x21, IDX_CONTEXT_X21)
    LOAD_REG_FROM_NEW(x22, IDX_CONTEXT_X22)
    LOAD_REG_FROM_NEW(x23, IDX_CONTEXT_X23)
    LOAD_REG_FROM_NEW(x24, IDX_CONTEXT_X24)
    LOAD_REG_FROM_NEW(x25, IDX_CONTEXT_X25)
    LOAD_REG_FROM_NEW(x26, IDX_CONTEXT_X26)
    LOAD_REG_FROM_NEW(x27, IDX_CONTEXT_X27)
    LOAD_REG_FROM_NEW(x28, IDX_CONTEXT_X28)
    LOAD_REG_FROM_NEW(x29, IDX_CONTEXT_X29)
    LOAD_REG_FROM_NEW(x30, IDX_CONTEXT_X30)

    ret
