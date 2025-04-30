# SPDX-License-Identifier: MIT

#include <arch/riscv/asm/registers.h>
#include <arch/riscv/asm/m_mode.h>

#define SAVE_REG_TO_STACK(REGISTER, OFFSET) SAVE_REG_TO_ARRAY(REGISTER, OFFSET, sp)
#define LOAD_REG_FROM_STACK(REGISTER, OFFSET) LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, sp)

#
# machine-mode timer interrupt.
#
# Sets the next timer and triggers a supervisor software interrupt.
#
# When triggered the hardware automatically performs the following as an atomic operation:
#  Fields in mstatus:
#   MPIE  = MIE - save current interrupt enable state
#   MIE   = 0   - disable interrupts
#   MPP   = current priviledge level
#  mcause = set to exception cause
#  mepc   = pc  - save program counter
#  pc     = mtvec - jump to m_mode_trap_vector
.globl m_mode_trap_vector
.align 4
m_mode_trap_vector:
    # exchange caller stack with M Mode stack
    csrrw sp, mscratch, sp

    # make room to save registers.
    addi sp, sp, -REG_COUNT_TO_SAVE_ALL*SIZE_OF_REG

    # save the registers.
    SAVE_REG_TO_STACK( ra, IDX_ALL_RA )
    # SAVE_REG_TO_STACK( sp, IDX_ALL_SP ) # sp is in mscratch, but can be omitted
    SAVE_REG_TO_STACK( gp, IDX_ALL_GP )
    SAVE_REG_TO_STACK( tp, IDX_ALL_TP )
    SAVE_REG_TO_STACK( t0, IDX_ALL_T0 )
    SAVE_REG_TO_STACK( t1, IDX_ALL_T1 )
    SAVE_REG_TO_STACK( t2, IDX_ALL_T2 )
    SAVE_REG_TO_STACK( fp, IDX_ALL_FP )
    SAVE_REG_TO_STACK( s1, IDX_ALL_S1 )
    SAVE_REG_TO_STACK( a0, IDX_ALL_A0 )
    SAVE_REG_TO_STACK( a1, IDX_ALL_A1 )
    SAVE_REG_TO_STACK( a2, IDX_ALL_A2 )
    SAVE_REG_TO_STACK( a3, IDX_ALL_A3 )
    SAVE_REG_TO_STACK( a4, IDX_ALL_A4 )
    SAVE_REG_TO_STACK( a5, IDX_ALL_A5 )
    SAVE_REG_TO_STACK( a6, IDX_ALL_A6 )
    SAVE_REG_TO_STACK( a7, IDX_ALL_A7 )
    SAVE_REG_TO_STACK( s2, IDX_ALL_S2 )
    SAVE_REG_TO_STACK( s3, IDX_ALL_S3 )
    SAVE_REG_TO_STACK( s4, IDX_ALL_S4 )
    SAVE_REG_TO_STACK( s5, IDX_ALL_S5 )
    SAVE_REG_TO_STACK( s6, IDX_ALL_S6 )
    SAVE_REG_TO_STACK( s7, IDX_ALL_S7 )
    SAVE_REG_TO_STACK( s8, IDX_ALL_S8 )
    SAVE_REG_TO_STACK( s9, IDX_ALL_S9 )
    SAVE_REG_TO_STACK( s10, IDX_ALL_S10 )
    SAVE_REG_TO_STACK( s11, IDX_ALL_S11 )
    SAVE_REG_TO_STACK( t3, IDX_ALL_T3 )
    SAVE_REG_TO_STACK( t4, IDX_ALL_T4 )
    SAVE_REG_TO_STACK( t5, IDX_ALL_T5 )
    SAVE_REG_TO_STACK( t6, IDX_ALL_T6 )

    # pass stack value to access (and modify) saved registers
    mv a0, sp
    call m_mode_interrupt_handler

    # restore all registers:
    LOAD_REG_FROM_STACK( ra, IDX_ALL_RA )
    # LOAD_REG_FROM_STACK( sp, IDX_ALL_SP ) # sp is in mscratch, but can be omitted
    LOAD_REG_FROM_STACK( gp, IDX_ALL_GP )
    LOAD_REG_FROM_STACK( tp, IDX_ALL_TP )
    LOAD_REG_FROM_STACK( t0, IDX_ALL_T0 )
    LOAD_REG_FROM_STACK( t1, IDX_ALL_T1 )
    LOAD_REG_FROM_STACK( t2, IDX_ALL_T2 )
    LOAD_REG_FROM_STACK( fp, IDX_ALL_FP )
    LOAD_REG_FROM_STACK( s1, IDX_ALL_S1 )
    LOAD_REG_FROM_STACK( a0, IDX_ALL_A0 )
    LOAD_REG_FROM_STACK( a1, IDX_ALL_A1 )
    LOAD_REG_FROM_STACK( a2, IDX_ALL_A2 )
    LOAD_REG_FROM_STACK( a3, IDX_ALL_A3 )
    LOAD_REG_FROM_STACK( a4, IDX_ALL_A4 )
    LOAD_REG_FROM_STACK( a5, IDX_ALL_A5 )
    LOAD_REG_FROM_STACK( a6, IDX_ALL_A6 )
    LOAD_REG_FROM_STACK( a7, IDX_ALL_A7 )
    LOAD_REG_FROM_STACK( s2, IDX_ALL_S2 )
    LOAD_REG_FROM_STACK( s3, IDX_ALL_S3 )
    LOAD_REG_FROM_STACK( s4, IDX_ALL_S4 )
    LOAD_REG_FROM_STACK( s5, IDX_ALL_S5 )
    LOAD_REG_FROM_STACK( s6, IDX_ALL_S6 )
    LOAD_REG_FROM_STACK( s7, IDX_ALL_S7 )
    LOAD_REG_FROM_STACK( s8, IDX_ALL_S8 )
    LOAD_REG_FROM_STACK( s9, IDX_ALL_S9 )
    LOAD_REG_FROM_STACK( s10, IDX_ALL_S10 )
    LOAD_REG_FROM_STACK( s11, IDX_ALL_S11 )
    LOAD_REG_FROM_STACK( t3, IDX_ALL_T3 )
    LOAD_REG_FROM_STACK( t4, IDX_ALL_T4 )
    LOAD_REG_FROM_STACK( t5, IDX_ALL_T5 )
    LOAD_REG_FROM_STACK( t6, IDX_ALL_T6 )

    addi sp, sp, REG_COUNT_TO_SAVE_ALL*SIZE_OF_REG
    csrrw sp, mscratch, sp

    # mret changes these fields in mstatus:
    #   MIE   = MPIE - restore previous interrupt enable state
    #   MPIE  = 1
    #   MPP   = 00
    #  pc     = mepc - jump back
    mret
