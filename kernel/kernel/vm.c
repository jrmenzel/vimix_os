/* SPDX-License-Identifier: MIT */

#include <arch/fence.h>
#include <arch/riscv/clint.h>
#include <arch/riscv/plic.h>
#include <kernel/elf.h>
#include <kernel/fs.h>
#include <kernel/kalloc.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>

//
// The kernel's page table: all memory is mapped to its real location.
pagetable_t g_kernel_pagetable;

extern char end_of_text[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[];  // u_mode_trap_vector.S

/// @brief Map MMIO device to kernel page.
/// @param k_pagetable Page table to add mapping to.
/// @param address Physical address which will also be the virtual address.
/// @param size Size of mapping in bytes.
void kvm_map_mmio(pagetable_t k_pagetable, size_t address, size_t size)
{
    kvm_map_or_panic(k_pagetable, address, address, size, PTE_R | PTE_W);
}

/// Make a direct-map page table for the kernel.
/// Here the "memory mapped devices" are mapped into kernel memory space
/// (once the created pagetable is used).
pagetable_t kvm_make_kernel_pagetable(char *end_of_memory)
{
    pagetable_t kpage_table = (pagetable_t)kalloc();
    memset(kpage_table, 0, PAGE_SIZE);

    // virtio mmio disk interface
    kvm_map_mmio(kpage_table, VIRTIO0, PAGE_SIZE);

    // uart registers
    kvm_map_mmio(kpage_table, UART0, PAGE_SIZE);

    // PLIC
    kvm_map_mmio(kpage_table, PLIC_BASE, PLIC_SIZE);

    // CLINT
    kvm_map_mmio(kpage_table, CLINT_BASE, CLINT_SIZE);

#ifdef RTC_GOLDFISH
    // RTC
    kvm_map_mmio(kpage_table, RTC_GOLDFISH, PAGE_SIZE);
#endif

#ifdef VIRT_TEST_BASE
    // shutdown qemu
    kvm_map_mmio(kpage_table, VIRT_TEST_BASE, PAGE_SIZE);
#endif

    // map kernel text executable and read-only.
    kvm_map_or_panic(kpage_table, KERNBASE, KERNBASE,
                     (size_t)end_of_text - KERNBASE, PTE_R | PTE_X);

    // map kernel data and the physical RAM we'll make use of.
    kvm_map_or_panic(kpage_table, (size_t)end_of_text, (size_t)end_of_text,
                     (size_t)end_of_memory - (size_t)end_of_text,
                     PTE_R | PTE_W);

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel.
    kvm_map_or_panic(kpage_table, TRAMPOLINE, (size_t)trampoline, PAGE_SIZE,
                     PTE_R | PTE_X);

    // map the ramdisk
    // kvm_map_or_panic(kpage_table, 0x82200000, 0x82200000,
    //                 0x82400000 - 0x82200000, PTE_R | PTE_W);

    // allocate and map a kernel stack for each process.
    init_per_process_kernel_stack(kpage_table);

    return kpage_table;
}

void kvm_init(char *end_of_memory)
{
    g_kernel_pagetable = kvm_make_kernel_pagetable(end_of_memory);
}

#if defined(_arch_is_32bit)
const size_t MAX_LEVELS_IN_PAGE_TABLE = 2;
const size_t MAX_PTES_PER_PAGE_TABLE = 1024;
#else
const size_t MAX_LEVELS_IN_PAGE_TABLE = 3;
const size_t MAX_PTES_PER_PAGE_TABLE = 512;
#endif

/// Return the address of the PTE in page table pagetable
/// that corresponds to virtual address va.  If alloc!=0,
/// create any required page-table pages.
///
/// The risc-v Sv39 scheme (64bit) has three levels of page-table
/// pages. A page-table page contains 512 64-bit PTEs.
/// A 64-bit virtual address is split into five fields:
///   39..63 -- must be zero.
///   30..38 -- 9 bits of level-2 index.
///   21..29 -- 9 bits of level-1 index.
///   12..20 -- 9 bits of level-0 index.
///    0..11 -- 12 bits of byte offset within the page.
///
/// The risc-v Sv32 scheme (32bit) has two levels of page-table
/// pages. A page-table page contains 1024 32-bit PTEs.
/// A 32-bit virtual address is split into three fields:
///   22..31 -- 10 bits of level-1 index.
///   12..21 -- 10 bits of level-0 index.
///    0..11 -- 12 bits of byte offset within the page.
pte_t *vm_walk(pagetable_t pagetable, size_t va, bool alloc)
{
#if defined(_arch_is_64bit)
    if (va >= MAXVA)
    {
        panic("vm_walk: virtual address is larger than supported");
    }
#endif

    for (size_t level = (MAX_LEVELS_IN_PAGE_TABLE - 1); level > 0; level--)
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
#if defined(_arch_is_64bit)
    if (va >= MAXVA)
    {
        return 0;
    }
#endif

    pte_t *pte = vm_walk(pagetable, va, false);
    if (pte == NULL) return 0;
    if ((*pte & PTE_V) == 0) return 0;
    if ((*pte & PTE_U) == 0) return 0;

    size_t pa = PTE2PA(*pte);
    return pa;
}

int32_t kvm_map_or_panic(pagetable_t k_pagetable, size_t va, size_t pa,
                         size_t size, int32_t perm)
{
    if (kvm_map(k_pagetable, va, pa, size, perm) != 0)
    {
        panic("kvm_map_or_panic failed");
    }
    return 0;
}

int32_t kvm_map(pagetable_t pagetable, size_t va, size_t pa, size_t size,
                int32_t perm)
{
    // printk("mapping (physical 0x%x) to 0x%x - 0x%x (size: %d pages)\n", pa,
    // va, va + size - 1,
    //        size / PAGE_SIZE);
    if (size == 0)
    {
        panic("kvm_map: size == 0");
    }

    size_t current_va = PAGE_ROUND_DOWN(va);
    size_t last_va = PAGE_ROUND_DOWN(va + size - 1);
    while (true)
    {
        pte_t *pte = vm_walk(pagetable, current_va, true);
        if (pte == NULL)
        {
            return -1;
        }
        if (*pte & PTE_V)
        {
            panic("kvm_map: remap");
        }
        *pte = PA2PTE(pa) | perm | PTE_V | PTE_A | PTE_D;
        if (current_va == last_va)
        {
            break;
        }
        current_va += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
    return 0;
}

void uvm_unmap(pagetable_t pagetable, size_t va, size_t npages, bool do_free)
{
    if ((va % PAGE_SIZE) != 0)
    {
        panic("uvm_unmap: not aligned");
    }

    for (size_t a = va; a < va + npages * PAGE_SIZE; a += PAGE_SIZE)
    {
        pte_t *pte = vm_walk(pagetable, a, false);
        if (pte == NULL)
        {
            panic("uvm_unmap: vm_walk");
        }
        if ((*pte & PTE_V) == 0)
        {
            panic("uvm_unmap: not mapped");
        }
        if (PTE_FLAGS(*pte) == PTE_V)
        {
            panic("uvm_unmap: not a leaf");
        }
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
    if (newsz < oldsz)
    {
        return oldsz;
    }

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
        if (kvm_map(pagetable, a, (size_t)mem, PAGE_SIZE,
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
    // there are 2^9 => 512 PTEs in a page table on 64 bit
    // there are 2^10 => 1024 PTEs in a page table on 32 bit
    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
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
    if (sz > 0)
    {
        uvm_unmap(pagetable, 0, PAGE_ROUND_UP(sz) / PAGE_SIZE, 1);
    }
    freewalk(pagetable);
}

int32_t uvm_copy(pagetable_t old, pagetable_t new, size_t sz)
{
    pte_t *pte;
    size_t i;

    bool fatal_error_happened = false;

    for (i = 0; i < sz; i += PAGE_SIZE)
    {
        pte = vm_walk(old, i, false);
        if (pte == NULL)
        {
            panic("uvm_copy: pte should exist");
        }
        if ((*pte & PTE_V) == 0)
        {
            panic("uvm_copy: page not present");
        }
        pte_t pa = PTE2PA(*pte);
        uint32_t flags = PTE_FLAGS(*pte);

        char *mem = kalloc();
        if (mem == NULL)
        {
            fatal_error_happened = true;
            break;
        }

        memmove(mem, (char *)pa, PAGE_SIZE);
        if (kvm_map(new, i, (size_t)mem, PAGE_SIZE, flags) != 0)
        {
            kfree(mem);
            fatal_error_happened = true;
            break;
        }
    }

    // might have copied paged with executable code, in that case flush the
    // instruction caches
    instruction_memory_barrier();

    if (fatal_error_happened)
    {
        uvm_unmap(new, 0, i / PAGE_SIZE, 1);
        return -1;
    }

    return 0;
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

int32_t uvm_copy_out(pagetable_t pagetable, size_t dst_va, char *src_pa,
                     size_t len)
{
    while (len > 0)
    {
        // copy up to one page each loop

        size_t dst_va_page_start = PAGE_ROUND_DOWN(dst_va);
        size_t dst_pa_page_start =
            uvm_get_physical_addr(pagetable, dst_va_page_start);
        if (dst_pa_page_start == 0)
        {
            return -1;
        }

        size_t dst_offset_in_page = dst_va - dst_va_page_start;
        size_t n = PAGE_SIZE - dst_offset_in_page;
        if (n > len)
        {
            n = len;
        }
        memmove((void *)(dst_pa_page_start + dst_offset_in_page), src_pa, n);

        len -= n;
        src_pa += n;
        dst_va = dst_va_page_start + PAGE_SIZE;
    }
    return 0;
}

int32_t uvm_copy_in(pagetable_t pagetable, char *dst_pa, size_t src_va,
                    size_t len)
{
    while (len > 0)
    {
        // copy up to one page each loop

        size_t src_va_page_start = PAGE_ROUND_DOWN(src_va);
        size_t src_pa_page_start =
            uvm_get_physical_addr(pagetable, src_va_page_start);
        if (src_pa_page_start == 0)
        {
            return -1;
        }

        size_t src_offset_in_page = src_va - src_va_page_start;
        size_t n = PAGE_SIZE - src_offset_in_page;
        if (n > len)
        {
            n = len;
        }
        memmove(dst_pa, (void *)(src_pa_page_start + src_offset_in_page), n);

        len -= n;
        dst_pa += n;
        src_va = src_va_page_start + PAGE_SIZE;
    }
    return 0;
}

int32_t uvm_copy_in_str(pagetable_t pagetable, char *dst_pa, size_t src_va,
                        size_t max)
{
    bool got_null = false;

    while (got_null == false && max > 0)
    {
        size_t src_va_page_start = PAGE_ROUND_DOWN(src_va);
        size_t src_pa_page_start =
            uvm_get_physical_addr(pagetable, src_va_page_start);
        if (src_pa_page_start == 0)
        {
            return -1;
        }

        size_t src_offset_in_page = src_va - src_va_page_start;
        size_t n = PAGE_SIZE - src_offset_in_page;
        if (n > max)
        {
            n = max;
        }

        char *src_pa = (char *)(src_pa_page_start + src_offset_in_page);
        while (n > 0)
        {
            if (*src_pa == '\0')
            {
                *dst_pa = '\0';
                got_null = true;
                break;
            }
            else
            {
                *dst_pa = *src_pa;
            }
            --n;
            --max;
            src_pa++;
            dst_pa++;
        }

        src_va = src_va_page_start + PAGE_SIZE;
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
