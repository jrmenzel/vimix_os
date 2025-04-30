/* SPDX-License-Identifier: MIT */
#if defined(CONFIG_RISCV_BOOT_M_MODE)

#define M_MODE_STACK 1024

#define MSTATUS_MBE (1L << 37)  //< M-Mode is Big Endian
#define MSTATUS_SBE (1L << 36)  //< S-Mode is Big Endian

#define MSTATUS_UXL_MASK \
    (3L << 32)  //< bit width in U-Mode (e.g. to allow 32 bit software to run on
                // 64 bit CPUs)
#define MSTATUS_SXL_MASK \
    (3L << 34)  //< bit width in S-Mode (e.g. to allow 32 bit OS to run on 64
                // bit CPUs)

#define MSTATUS_SUM  //< S-Mode can access U-Mode pages

// Machine Status Register, mstatus
#define MSTATUS_MPP_MASK (3L << 11)  //< previous mode mask
#define MSTATUS_MPP_M (3L << 11)     //< mret will go to Machine Mode
#define MSTATUS_MPP_S (1L << 11)     //< mret will go to Supervisor Mode
#define MSTATUS_MPP_U (0L << 11)     //< mret will go to User Mode

#define MSTATUS_VS_MASK (3L << 9)  //<

#define MSTATUS_SPP \
    (1L << 8)  //< previous mode before s-mode exception: 0= from u-mode, 1 =
               // from s-mode
#define MSTATUS_MPIE (1L << 7)  //< m-mode interrupt enable before the exception
#define MSTATUS_UBE (1L << 6)   //< U-Mode is Big Endian
#define MSTATUS_SPIE (1L << 5)  //< s-mode interrupt enable before the exception
#define MSTATUS_MIE (1L << 3)   //< m-mode interrupt enable
#define MSTATUS_SIE (1L << 1)   //< s-mode interrupt enable

// mie csr: Interrupt Enable
#define MIE_MEIE (1L << 11)  //< m-mode external
#define MIE_SEIE (1L << 9)   //< s-mode external
#define MIE_MTIE (1L << 7)   //< m-mode timer
#define MIE_STIE (1L << 5)   //< s-mode timer
#define MIE_MSIE (1L << 3)   //< m-mode software
#define MIE_SSIE (1L << 1)   //< s-mode software

#if (__riscv_xlen == 32)
#define MCAUSE_INTERRUPT (1 << 31)
#else
// 64 bit
#define MCAUSE_INTERRUPT (1UL << 63)
#endif

#define MCAUSE_MACHINE_SOFTWARE (MCAUSE_INTERRUPT | 3)
#define MCAUSE_MACHINE_TIMER (MCAUSE_INTERRUPT | 7)
#define MCAUSE_ILLEGAL_INSTRUCTION (2)
#define MCAUSE_ECALL_FROM_U_MODE (8)
#define MCAUSE_ECALL_FROM_S_MODE (9)

#define INT_CAUSE_NONE 0
#define INT_CAUSE_START 1

#ifndef __ASSEMBLY__

#include "riscv.h"
// machine mode CSR read / write functions

rv_read_csr_(mhartid);  // hard-id

rv_read_csr_(pmpcfg0);  // Physical Memory Protection
rv_write_csr_(pmpcfg0);

rv_read_csr_(mstatus);
rv_write_csr_(mstatus);

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
rv_read_csr_(mepc);
rv_write_csr_(mepc);

rv_read_csr_(mtval);
rv_write_csr_(mtval);

rv_read_csr_(mcause);
rv_write_csr_(mcause);

rv_read_csr_(mie);  // machine-mode Interrupt Enable
rv_write_csr_(mie);

// Machine Exception Delegation
rv_read_csr_(medeleg);
rv_write_csr_(medeleg);

// medleg is a 64 bit register (32 bit RISC V uses medlegh for the top 32 bit)
// but everything above bit 15 is surrently reserved or unhandled by VIMIX
// anyways. One bit per delegation, the bit position matches the mcause value
// (not the same bit pattern!)
#define MEDLEG_ALL 0xFFFF

#define MEDLELEG_ILLEGAL_INSTRUCTION (1 << MCAUSE_ILLEGAL_INSTRUCTION)

/// Set this bit to delegate environment calls from U mode (syscalls) to S mode
#define MEDLELEG_ECALL_FROM_U_MODE (1 << MCAUSE_ECALL_FROM_U_MODE)

/// Unset this bit to handle environment calls from S mode in M mode
#define MEDLELEG_ECALL_FROM_S_MODE (1 << MCAUSE_ECALL_FROM_S_MODE)

// Machine Interrupt Delegation
rv_read_csr_(mideleg);
rv_write_csr_(mideleg);

rv_read_csr_(mscratch);
rv_write_csr_(mscratch);

// Machine mode trap vectors
rv_read_csr_(mtvec);
rv_write_csr_(mtvec);

// Machine-mode Counter-Enable
rv_read_csr_(mcounteren);
rv_write_csr_(mcounteren);

// physical memory protection: register 0 is the range END, register 1 is the
// start
rv_write_csr_(pmpaddr0);
rv_write_csr_(pmpaddr1);

// environment config register
rv_read_csr_(menvcfg);
rv_write_csr_(menvcfg);
#if (__riscv_xlen == 32)
rv_read_csr_(menvcfgh);
rv_write_csr_(menvcfgh);
#endif

#endif  // __ASSEMBLY__

#endif  // CONFIG_RISCV_BOOT_M_MODE
