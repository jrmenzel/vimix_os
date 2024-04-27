# SPDX-License-Identifier: MIT

# Context switch
#
#   void context_switch(struct context *old, struct context *new);
# 
# Save current registers in old (a0). Load from new (a1).

#if (__riscv_xlen == 32)
#define SAVE_REG_TO_OLD(REGISTER, OFFSET) sw REGISTER, (OFFSET*4)(a0)
#define LOAD_REG_FROM_NEW(REGISTER, OFFSET) lw REGISTER, (OFFSET*4)(a1)
#else
# 64 bit
#define SAVE_REG_TO_OLD(REGISTER, OFFSET) sd REGISTER, (OFFSET*8)(a0)
#define LOAD_REG_FROM_NEW(REGISTER, OFFSET) ld REGISTER, (OFFSET*8)(a1)
#endif

.globl context_switch
context_switch:
    SAVE_REG_TO_OLD(ra, 0)
    SAVE_REG_TO_OLD(sp, 1)
    SAVE_REG_TO_OLD(s0, 2)
    SAVE_REG_TO_OLD(s1, 3)
    SAVE_REG_TO_OLD(s2, 4)
    SAVE_REG_TO_OLD(s3, 5)
    SAVE_REG_TO_OLD(s4, 6)
    SAVE_REG_TO_OLD(s5, 7)
    SAVE_REG_TO_OLD(s6, 8)
    SAVE_REG_TO_OLD(s7, 9)
    SAVE_REG_TO_OLD(s8, 10)
    SAVE_REG_TO_OLD(s9, 11)
    SAVE_REG_TO_OLD(s10, 12)
    SAVE_REG_TO_OLD(s11, 13)

    LOAD_REG_FROM_NEW(ra, 0)
    LOAD_REG_FROM_NEW(sp, 1)
    LOAD_REG_FROM_NEW(s0, 2)
    LOAD_REG_FROM_NEW(s1, 3)
    LOAD_REG_FROM_NEW(s2, 4)
    LOAD_REG_FROM_NEW(s3, 5)
    LOAD_REG_FROM_NEW(s4, 6)
    LOAD_REG_FROM_NEW(s5, 7)
    LOAD_REG_FROM_NEW(s6, 8)
    LOAD_REG_FROM_NEW(s7, 9)
    LOAD_REG_FROM_NEW(s8, 10)
    LOAD_REG_FROM_NEW(s9, 11)
    LOAD_REG_FROM_NEW(s10, 12)
    LOAD_REG_FROM_NEW(s11, 13)
    ret

