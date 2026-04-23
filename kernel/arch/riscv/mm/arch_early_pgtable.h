/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <kernel/kernel.h>

static inline size_t satp_make_reg_value(size_t addr_of_first_block,
                                         uint32_t asid)
{
    xlen_t satp = 0;
    if (addr_of_first_block != 0)
    {
        satp = (addr_of_first_block >> PAGE_SHIFT) & SATP_PPN_MASK;
        satp = satp | SATP_MODE;
        satp = satp | (((xlen_t)asid << SATP_ASID_POS) & SATP_ASID_MASK);
    }

    return satp;
}
