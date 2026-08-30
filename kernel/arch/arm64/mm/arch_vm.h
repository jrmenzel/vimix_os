/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/arm64/arm64.h>
#include <arch/arm64/asm/arm64_def.h>
#include <arch/barrier.h>
#include <kernel/kernel.h>
#include <mm/pte.h>

/// @brief The kernel page table is always mapped, no special handling is
/// needed.
#define CAN_BE_CALLED_ON_USER_PAGE_TABLE

static inline void mmu_set_kernel_pgtable_reg(size_t reg_value)
{
    arm_write_ttbr1_el1(reg_value);
    dsb(ish);
    isb();
}

static inline void mmu_set_user_pgtable_reg(size_t reg_value)
{
    arm_write_ttbr0_el1(reg_value);
    dsb(ish);
    isb();
}

static inline void mmu_flush_tlb()
{
    // Ensure prior page table updates are visible:
    dsb(ishst);
    // flush all TLB entries:
    asm volatile("tlbi vmalle1");
    dsb(ish);
    isb();
}

static inline void mmu_flush_tlb_asid(uint32_t asid)
{
    size_t tlbi_param = (((size_t)asid << TTBR_ASID_POS) & TTBR_ASID_MASK);

    // Ensure prior page table updates are visible:
    dsb(ishst);
    // flush all TLB entries of ASID:
    asm volatile("tlbi aside1, %0" : : "r"(tlbi_param));
    dsb(ish);
    isb();
}
