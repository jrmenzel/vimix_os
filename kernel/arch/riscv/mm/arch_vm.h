/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/asm/satp.h>
#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>
#include <mm/pte.h>

/// @brief To allow traps from user mode some functions are called when the
/// kernel is still on the user page table (or again during return to user mode)
/// Mark these to link them into the trampsec page.
#define CAN_BE_CALLED_ON_USER_PAGE_TABLE __attribute__((section("trampsec")))

CAN_BE_CALLED_ON_USER_PAGE_TABLE static inline void mmu_set_kernel_pgtable_reg(
    size_t reg_value)
{
    asm volatile("csrw satp, %0" ::"r"(reg_value) : "memory");
}

CAN_BE_CALLED_ON_USER_PAGE_TABLE static inline void mmu_set_user_pgtable_reg(
    size_t reg_value)
{
    asm volatile("csrw satp, %0" ::"r"(reg_value) : "memory");
}

CAN_BE_CALLED_ON_USER_PAGE_TABLE static inline void mmu_flush_tlb()
{
// flush TLB if zifencei extension is supported, noop otherwise
#if defined(__RISCV_EXT_ZIFENCEI)
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
#else
    _Static_assert(
        CPUS == 1,
        "mmu_flush_tlb is not implemented for multi-core without zifencei");
#endif
}

CAN_BE_CALLED_ON_USER_PAGE_TABLE static inline void mmu_flush_tlb_asid(
    uint32_t asid)
{
// flush TLB if zifencei extension is supported, noop otherwise
#if defined(__RISCV_EXT_ZIFENCEI)
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, %0" ::"r"(asid));
#else
    _Static_assert(
        CPUS == 1,
        "mmu_flush_tlb is not implemented for multi-core without zifencei");
#endif
}
