/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/arm64/asm/arm64_def.h>
#include <kernel/types.h>

// read a special register:
#define arm_read_(name)                           \
    static inline size_t arm_read_##name()        \
    {                                             \
        size_t x;                                 \
        asm volatile("mrs %0, " #name : "=r"(x)); \
        return x;                                 \
    }

// write a special register:
#define arm_write_(name)                                         \
    static inline void arm_write_##name(size_t x)                \
    {                                                            \
        asm volatile("msr " #name ", %0" : : "r"(x) : "memory"); \
    }

/// @brief Reads the Exception Syndrome Register
arm_read_(esr_el1);

#define ESR_GET_EXC_CLASS(esr) (esr >> 26)
#define ESR_EC_UNKNOWN 0x00
#define ESR_EC_WFI_WFE 0x01
#define ESR_EC_CP15_RT 0x03
#define ESR_EC_CP15_RRT 0x04
#define ESR_EC_CP14_RT 0x05
#define ESR_EC_CP14_RRT 0x06
#define ESR_EC_ILLEGAL_STATE 0x0E
#define ESR_EC_SVC_A64 0x15  // SVC / syscall from AArch64
#define ESR_EC_HVC_A64 0x16  // HVC from AArch64
#define ESR_EC_SMC_A64 0x17  // SMC from AArch64
#define ESR_EC_SYSREG 0x18   // MSR/MRS/SYS instruction
#define ESR_EC_INSN_ABORT_EL0 0x20
#define ESR_EC_INSN_ABORT_EL1 0x21
#define ESR_EC_DATA_ABORT_EL0 0x24
#define ESR_EC_DATA_ABORT_EL1 0x25
#define ESR_EC_PC_ALIGNMENT_FAULT 0x22
#define ESR_EC_SP_ALIGNMENT_FAULT 0x26
#define ESR_EC_FP_TRAP_A64 0x2C
#define ESR_EC_SERROR 0x2F
#define ESR_EC_BRK 0x3C  // Breakpoint
#define ESR_EC_VECTOR_CATCH 0x1A
#define ESR_EC_BRK_A64 0x3C  // BRK instruction from AArch64

/// Data Fault Status Code mask
#define ESR_INST_FAULT_DFSC_MASK 0x3F
#define ESR_GET_DFSC(esr) (esr & ESR_INST_FAULT_DFSC_MASK)
/// "write not read"
#define ESR_DATA_FAULT_WNR 0x20
/// Was the fault caused during a page table walk? indicated issues with the
/// page table itself
#define ESR_DATA_FAULT_S1PTW 0x2000

/// Instruction Fault Status Code mask
#define ESR_INST_FAULT_IFSC_MASK 0x3F
#define ESR_GET_IFSC(esr) (esr & ESR_INST_FAULT_IFSC_MASK)
#define ESR_INST_FAULT_S1PTW (1 << 7)
#define ESR_INST_FAULT_FNV (1 << 10)
#define ESR_IFSC_SYNC_EXT_ABORT 0b010000

/// @brief Reads the exception return register
arm_read_(elr_el1);
arm_write_(elr_el1);

/// @brief Reads saved program state (CPU flags) from before the exception
arm_read_(spsr_el1);
arm_write_(spsr_el1);

/// @brief Reads fault address register (offending address on page faults)
arm_read_(far_el1);

/// @brief Counter-timer Virtual Timer Control Register
arm_read_(cntv_ctl_el0);
arm_write_(cntv_ctl_el0);

#define CNTV_CTL_ENABLE (1UL << 0)
#define CNTV_CTL_IMASK (1UL << 1)
#define CNTV_CTL_ISTATUS (1UL << 2)

/// @brief Cache Type Register
arm_read_(ctr_el0);

/// @brief Thread ID register
arm_read_(tpidr_el1);

arm_read_(daif);

#define DAIF_STATE_FIQ (1UL << 6)
#define DAIF_STATE_IRQ (1UL << 7)

arm_read_(cntpct_el0);
arm_read_(cntfrq_el0);

/// @brief Kernel page table (upper half of address space)
arm_read_(ttbr1_el1);
arm_write_(ttbr1_el1);

/// @brief User page table (lower half of address space)
arm_write_(ttbr0_el1);

/// @brief User / EL0 stack pointer
arm_write_(sp_el0);

arm_write_(vbar_el1);
arm_write_(cntv_tval_el0);
