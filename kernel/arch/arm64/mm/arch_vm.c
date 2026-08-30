/* SPDX-License-Identifier: MIT */

#include <arch/arm64/arm64.h>
#include <kernel/kernel.h>
#include <kernel/pgtable.h>
#include <mm/pte.h>

size_t mmu_get_kernel_pgtable_reg_value() { return arm_read_ttbr1_el1(); }

size_t mmu_get_page_table_address(size_t reg_value)
{
    return (reg_value & ~TTBR_ASID_MASK);
}

size_t mmu_get_page_table_asid(size_t reg_value)
{
    return (reg_value & TTBR_ASID_MASK) >> TTBR_ASID_POS;
}

size_t mmu_make_page_table_reg_pa(size_t phys_addr_of_first_block,
                                  uint32_t asid)
{
    size_t value = (phys_addr_of_first_block & ~TTBR_ASID_MASK) |
                   (((size_t)asid << TTBR_ASID_POS) & TTBR_ASID_MASK);
    value |= TTBR_CNP;
    return value;
}
