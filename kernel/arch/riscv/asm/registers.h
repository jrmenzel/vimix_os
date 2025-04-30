/* SPDX-License-Identifier: MIT */

// defines the order in which registers are saved on stack in the S-Mode trap
// vector. Also used to print the caller registers in case of an unexpected
// interrupt.
#define IDX_RA 0
#define IDX_GP 1
#define IDX_TP 2
#define IDX_T0 3
#define IDX_T1 4
#define IDX_T2 5
#define IDX_A0 6
#define IDX_A1 7
#define IDX_A2 8
#define IDX_A3 9
#define IDX_A4 10
#define IDX_A5 11
#define IDX_A6 12
#define IDX_A7 13
#define IDX_T3 14
#define IDX_T4 15
#define IDX_T5 16
#define IDX_T6 17

#define REG_COUNT_TO_SAVE 18

// M Mode interrupt handler saves all registers to enable illegal instruction
// handlers to save to any register. Order must be in the order the registers
// are encoded in the instructions (encoding-1 as register zero is not saved)

#define IDX_ALL_RA 0
#define IDX_ALL_SP 1
#define IDX_ALL_GP 2
#define IDX_ALL_TP 3
#define IDX_ALL_T0 4
#define IDX_ALL_T1 5
#define IDX_ALL_T2 6
#define IDX_ALL_FP 7
#define IDX_ALL_S1 8
#define IDX_ALL_A0 9
#define IDX_ALL_A1 10
#define IDX_ALL_A2 11
#define IDX_ALL_A3 12
#define IDX_ALL_A4 13
#define IDX_ALL_A5 14
#define IDX_ALL_A6 15
#define IDX_ALL_A7 16
#define IDX_ALL_S2 17
#define IDX_ALL_S3 18
#define IDX_ALL_S4 19
#define IDX_ALL_S5 20
#define IDX_ALL_S6 21
#define IDX_ALL_S7 22
#define IDX_ALL_S8 23
#define IDX_ALL_S9 24
#define IDX_ALL_S10 25
#define IDX_ALL_S11 26
#define IDX_ALL_T3 27
#define IDX_ALL_T4 28
#define IDX_ALL_T5 29
#define IDX_ALL_T6 30

#define REG_COUNT_TO_SAVE_ALL 31

#if (__riscv_xlen == 32)
#define SIZE_OF_REG 4
#define SAVE_REG_TO_ARRAY(REGISTER, OFFSET, REG) \
    sw REGISTER, (OFFSET * SIZE_OF_REG)(REG)
#define LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, REG) \
    lw REGISTER, (OFFSET * SIZE_OF_REG)(REG)
#else
// 64 bit
#define SIZE_OF_REG 8
#define SAVE_REG_TO_ARRAY(REGISTER, OFFSET, REG) \
    sd REGISTER, (OFFSET * SIZE_OF_REG)(REG)
#define LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, REG) \
    ld REGISTER, (OFFSET * SIZE_OF_REG)(REG)
#endif
