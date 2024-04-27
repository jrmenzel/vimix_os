/* SPDX-License-Identifier: MIT */

#include <kernel/elf.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>

//
// The kernel's page table: all memory is mapped to its real location.
pagetable_t kernel_pagetable;

extern char end_of_text[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[];  // u_mode_trap_vector.S

/// Make a direct-map page table for the kernel.
/// Here the "memory mapped devices" are mapped into kernel memory space
/// (once the created pagetable is used).
pagetable_t kvm_make_kernel_pagetable()
{
    pagetable_t kpage_table = (pagetable_t)kalloc();
    memset(kpage_table, 0, PAGE_SIZE);

    // uart registers
    kvm_map_or_panic(kpage_table, UART0, UART0, PAGE_SIZE, PTE_R | PTE_W);

    // virtio mmio disk interface
    kvm_map_or_panic(kpage_table, VIRTIO0, VIRTIO0, PAGE_SIZE, PTE_R | PTE_W);

    // PLIC
    kvm_map_or_panic(kpage_table, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    // map kernel text executable and read-only.
    kvm_map_or_panic(kpage_table, KERNBASE, KERNBASE,
                     (size_t)end_of_text - KERNBASE, PTE_R | PTE_X);

    // map kernel data and the physical RAM we'll make use of.
    kvm_map_or_panic(kpage_table, (size_t)end_of_text, (size_t)end_of_text,
                     PHYSTOP - (size_t)end_of_text, PTE_R | PTE_W);

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel.
    kvm_map_or_panic(kpage_table, TRAMPOLINE, (size_t)trampoline, PAGE_SIZE,
                     PTE_R | PTE_X);

    // allocate and map a kernel stack for each process.
    init_per_process_kernel_stack(kpage_table);

    return kpage_table;
}

void kvm_init() { kernel_pagetable = kvm_make_kernel_pagetable(); }

void kvm_init_per_cpu()
{
    // wait for any previous writes to the page table memory to finish.
    rv_sfence_vma();

    cpu_set_page_table(MAKE_SATP(kernel_pagetable));

    // flush stale entries from the TLB.
    rv_sfence_vma();
}

pte_t *vm_walk(pagetable_t pagetable, size_t va, bool alloc)
{
    if (va >= MAXVA) panic("vm_walk");

    for (size_t level = 2; level > 0; level--)
    {
        pte_t *pte = &pagetable[PAGE_TABLE_INDEX(level, va)];
        if (*pte & PTE_V)
        {
            pagetable = (pagetable_t)PTE2PA(*pte);
        }
        else
        {
            if (!alloc || (pagetable = (pagetable_t)kalloc()) == NULL)
            {
                return NULL;
            }
            memset(pagetable, 0, PAGE_SIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PAGE_TABLE_INDEX(0, va)];
}

/// Look up a virtual address, return the physical address,
/// or 0 if not mapped.
/// Can only be used to look up user pages.
size_t uvm_get_physical_addr(pagetable_t pagetable, size_t va)
{
    if (va >= MAXVA)
    {
        return 0;
    }

    pte_t *pte = vm_walk(pagetable, va, 0);
    if (pte == NULL) return 0;
    if ((*pte & PTE_V) == 0) return 0;
    if ((*pte & PTE_U) == 0) return 0;
    size_t pa = PTE2PA(*pte);
    return pa;
}

int32_t kvm_map_or_panic(pagetable_t k_pagetable, size_t va, size_t pa,
                         size_t sz, int32_t perm)
{
    if (kvm_map(k_pagetable, va, sz, pa, perm) != 0)
    {
        panic("kvm_map_or_panic failed");
    }
    return 0;
}

int32_t kvm_map(pagetable_t pagetable, size_t va, size_t size, size_t pa,
                int32_t perm)
{
    if (size == 0)
    {
        panic("kvm_map: size == 0");
    }

    size_t a = PAGE_ROUND_DOWN(va);
    size_t last = PAGE_ROUND_DOWN(va + size - 1);
    while (true)
    {
        pte_t *pte;
        if ((pte = vm_walk(pagetable, a, true)) == NULL) return -1;
        if (*pte & PTE_V) panic("kvm_map: remap");
        *pte = PA2PTE(pa) | perm | PTE_V;
        if (a == last) break;
        a += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
    return 0;
}

void uvm_unmap(pagetable_t pagetable, size_t va, size_t npages, bool do_free)
{
    pte_t *pte;

    if ((va % PAGE_SIZE) != 0) panic("uvm_unmap: not aligned");

    for (size_t a = va; a < va + npages * PAGE_SIZE; a += PAGE_SIZE)
    {
        if ((pte = vm_walk(pagetable, a, 0)) == NULL)
            panic("uvm_unmap: vm_walk");
        if ((*pte & PTE_V) == 0) panic("uvm_unmap: not mapped");
        if (PTE_FLAGS(*pte) == PTE_V) panic("uvm_unmap: not a leaf");
        if (do_free)
        {
            size_t pa = PTE2PA(*pte);
            kfree((void *)pa);
        }
        *pte = 0;
    }
}

pagetable_t uvm_create()
{
    pagetable_t pagetable = (pagetable_t)kalloc();
    if (pagetable == NULL)
    {
        return NULL;
    }
    memset(pagetable, 0, PAGE_SIZE);
    return pagetable;
}

/// Allocate PTEs and physical memory to grow process from oldsz to
/// newsz, which need not be page aligned.  Returns new size or 0 on error.
size_t uvm_alloc(pagetable_t pagetable, size_t oldsz, size_t newsz,
                 int32_t xperm)
{
    if (newsz < oldsz) return oldsz;

    oldsz = PAGE_ROUND_UP(oldsz);
    for (size_t a = oldsz; a < newsz; a += PAGE_SIZE)
    {
        char *mem = kalloc();
        if (mem == 0)
        {
            uvm_dealloc(pagetable, a, oldsz);
            return 0;
        }
        memset(mem, 0, PAGE_SIZE);
        if (kvm_map(pagetable, a, PAGE_SIZE, (size_t)mem,
                    PTE_R | PTE_U | xperm) != 0)
        {
            kfree(mem);
            uvm_dealloc(pagetable, a, oldsz);
            return 0;
        }
    }
    return newsz;
}

size_t uvm_dealloc(pagetable_t pagetable, size_t oldsz, size_t newsz)
{
    if (newsz >= oldsz)
    {
        return oldsz;
    }

    if (PAGE_ROUND_UP(newsz) < PAGE_ROUND_UP(oldsz))
    {
        size_t npages =
            (PAGE_ROUND_UP(oldsz) - PAGE_ROUND_UP(newsz)) / PAGE_SIZE;
        uvm_unmap(pagetable, PAGE_ROUND_UP(newsz), npages, 1);
    }

    return newsz;
}

/// Recursively free page-table pages.
/// All leaf mappings must already have been removed.
void freewalk(pagetable_t pagetable)
{
    // there are 2^9 = 512 PTEs in a page table.
    for (size_t i = 0; i < 512; i++)
    {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
        {
            // this PTE points to a lower-level page table.
            size_t child = PTE2PA(pte);
            freewalk((pagetable_t)child);
            pagetable[i] = 0;
        }
        else if (pte & PTE_V)
        {
            panic("freewalk: leaf");
        }
    }
    kfree((void *)pagetable);
}

void uvm_free(pagetable_t pagetable, size_t sz)
{
    if (sz > 0) uvm_unmap(pagetable, 0, PAGE_ROUND_UP(sz) / PAGE_SIZE, 1);
    freewalk(pagetable);
}

int32_t uvm_copy(pagetable_t old, pagetable_t new, size_t sz)
{
    pte_t *pte;
    size_t pa, i;
    char *mem;

    for (i = 0; i < sz; i += PAGE_SIZE)
    {
        if ((pte = vm_walk(old, i, 0)) == NULL)
            panic("uvm_copy: pte should exist");
        if ((*pte & PTE_V) == 0) panic("uvm_copy: page not present");
        pa = PTE2PA(*pte);
        uint32_t flags = PTE_FLAGS(*pte);
        if ((mem = kalloc()) == 0) goto err;
        memmove(mem, (char *)pa, PAGE_SIZE);
        if (kvm_map(new, i, PAGE_SIZE, (size_t)mem, flags) != 0)
        {
            kfree(mem);
            goto err;
        }
    }
    return 0;

err:
    uvm_unmap(new, 0, i / PAGE_SIZE, 1);
    return -1;
}

void uvm_clear(pagetable_t pagetable, size_t va)
{
    pte_t *pte = vm_walk(pagetable, va, false);
    if (pte == NULL)
    {
        panic("uvm_clear");
    }
    *pte &= ~PTE_U;
}

int32_t uvm_copy_out(pagetable_t pagetable, size_t dstva, char *src, size_t len)
{
    size_t n, va0, pa0;

    while (len > 0)
    {
        va0 = PAGE_ROUND_DOWN(dstva);
        pa0 = uvm_get_physical_addr(pagetable, va0);
        if (pa0 == 0) return -1;
        n = PAGE_SIZE - (dstva - va0);
        if (n > len) n = len;
        memmove((void *)(pa0 + (dstva - va0)), src, n);

        len -= n;
        src += n;
        dstva = va0 + PAGE_SIZE;
    }
    return 0;
}

int32_t uvm_copy_in(pagetable_t pagetable, char *dst, size_t srcva, size_t len)
{
    size_t n, va0, pa0;

    while (len > 0)
    {
        va0 = PAGE_ROUND_DOWN(srcva);
        pa0 = uvm_get_physical_addr(pagetable, va0);
        if (pa0 == 0) return -1;
        n = PAGE_SIZE - (srcva - va0);
        if (n > len) n = len;
        memmove(dst, (void *)(pa0 + (srcva - va0)), n);

        len -= n;
        dst += n;
        srcva = va0 + PAGE_SIZE;
    }
    return 0;
}

int32_t uvm_copy_in_str(pagetable_t pagetable, char *dst, size_t srcva,
                        size_t max)
{
    size_t n, va0, pa0;
    bool got_null = false;

    while (got_null == false && max > 0)
    {
        va0 = PAGE_ROUND_DOWN(srcva);
        pa0 = uvm_get_physical_addr(pagetable, va0);
        if (pa0 == 0) return -1;
        n = PAGE_SIZE - (srcva - va0);
        if (n > max) n = max;

        char *p = (char *)(pa0 + (srcva - va0));
        while (n > 0)
        {
            if (*p == '\0')
            {
                *dst = '\0';
                got_null = true;
                break;
            }
            else
            {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }

        srcva = va0 + PAGE_SIZE;
    }
    if (got_null)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
