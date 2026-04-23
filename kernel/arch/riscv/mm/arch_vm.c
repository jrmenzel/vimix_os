/* SPDX-License-Identifier: MIT */

#include <kernel/pgtable.h>
#include <kernel/proc.h>
#include <mm/arch_early_pgtable.h>
#include <mm/vm.h>
#include "asm/satp.h"
#include "riscv.h"

size_t mmu_get_page_table_reg_value() { return rv_read_csr_satp(); }

size_t mmu_make_page_table_reg_pa(size_t phys_addr_of_first_block,
                                  uint32_t asid)
{
    if (asid > SATP_ASID_MAX)
    {
        panic("ASID too large");
    }

    return satp_make_reg_value(phys_addr_of_first_block, asid);
}

size_t mmu_make_page_table_reg(size_t addr_of_first_block, uint32_t asid)
{
    return mmu_make_page_table_reg_pa(virt_to_phys(addr_of_first_block), asid);
}

size_t mmu_get_page_table_address(size_t reg_value)
{
    reg_value = reg_value & SATP_PPN_MASK;
    return reg_value << PAGE_SHIFT;
}

size_t mmu_get_page_table_asid(size_t reg_value)
{
    reg_value = reg_value & SATP_ASID_MASK;
    return reg_value >> SATP_ASID_POS;
}
