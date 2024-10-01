/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <mm/pte.h>

/// Switch h/w page table register to the kernel's page table
/// and enable paging.
void kvm_init_per_cpu();

static inline void debug_vm_print_pte_flags(size_t flags)
{
    printk("%c%c%c%c%c%c%c%c", (flags & PTE_V) ? 'v' : '_',
           (flags & PTE_R) ? 'r' : '_', (flags & PTE_W) ? 'w' : '_',
           (flags & PTE_X) ? 'x' : '_', (flags & PTE_U) ? 'u' : '_',
           (flags & PTE_G) ? 'g' : '_', (flags & PTE_A) ? 'a' : '_',
           (flags & PTE_D) ? 'd' : '_');
}
