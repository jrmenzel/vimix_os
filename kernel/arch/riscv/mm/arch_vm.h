/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <mm/pte.h>
#include "asm/satp.h"

#define DEBUG_VM_PRINT_ARCH_PTE_FLAGS(pte) \
    printk("%c", PTE_IS_DIRTY(flags) ? 'd' : '_');

static inline void mmu_flush_tlb()
{
// flush TLB if zifencei extension is supported, noop otherwise
#if defined(__RISCV_EXT_ZIFENCEI)
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
#endif
}

static inline void mmu_flush_tlb_asid(uint32_t asid)
{
// flush TLB if zifencei extension is supported, noop otherwise
#if defined(__RISCV_EXT_ZIFENCEI)
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, %0" ::"r"(asid));
#endif
}
