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
