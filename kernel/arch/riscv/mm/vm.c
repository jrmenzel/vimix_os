/* SPDX-License-Identifier: MIT */

#include "mm/vm.h"
#include <kernel/proc.h>
#include "asm/satp.h"
#include "riscv.h"

size_t mmu_get_page_table_reg_value() { return rv_read_csr_satp(); }

size_t mmu_make_page_table_reg(size_t addr_of_first_block, uint32_t asid)
{
    if (asid > SATP_ASID_MAX)
    {
        panic("ASID too large");
    }

    xlen_t satp = 0;
    if (addr_of_first_block != 0)
    {
        satp = (addr_of_first_block >> PAGE_SHIFT) & SATP_PPN_MASK;
        satp = satp | SATP_MODE;
        satp = satp | (((xlen_t)asid << SATP_ASID_POS) & SATP_ASID_MASK);
    }

    return satp;
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
