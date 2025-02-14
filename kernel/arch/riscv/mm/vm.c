/* SPDX-License-Identifier: MIT */

#include "mm/vm.h"
#include <kernel/proc.h>
#include "riscv.h"

extern pagetable_t g_kernel_pagetable;

void kvm_init_per_cpu() { cpu_set_page_table(MAKE_SATP(g_kernel_pagetable)); }

pte_t elf_flags_to_perm(int32_t flags)
{
    pte_t perm = 0;
    if (flags & 0x1) perm = PTE_X;
    if (flags & 0x2) perm |= PTE_W;
    return perm;
}
