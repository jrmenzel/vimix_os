/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <mm/pte.h>
#include "asm/satp.h"

/// @brief To allow traps from user mode some functions are called when the
/// kernel is still on the user page table (or again during return to user mode)
/// Mark these to link them into the trampsec page.
#define CAN_BE_CALLED_ON_USER_PAGE_TABLE __attribute__((section("trampsec")))

#define DEBUG_VM_PRINT_ARCH_PTE_FLAGS(pte) \
    printk("%c", PTE_IS_DIRTY(flags) ? 'd' : '_');

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
#endif
}

CAN_BE_CALLED_ON_USER_PAGE_TABLE static inline void mmu_flush_tlb_asid(
    uint32_t asid)
{
// flush TLB if zifencei extension is supported, noop otherwise
#if defined(__RISCV_EXT_ZIFENCEI)
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, %0" ::"r"(asid));
#endif
}

/// call after changing executable code in memory to flush instruction caches
CAN_BE_CALLED_ON_USER_PAGE_TABLE static inline void
mmu_flush_instruction_cache()
{
    asm volatile("fence.i");
}
