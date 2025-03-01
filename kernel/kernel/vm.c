/* SPDX-License-Identifier: MIT */

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
#include <mm/mm.h>

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
    kvm_map_or_panic(k_pagetable, address, address, size, PTE_MMIO_FLAGS);
}

/// Make a direct-map page table for the kernel.
/// Here the "memory mapped devices" are mapped into kernel memory space
/// (once the created pagetable is used).
pagetable_t kvm_make_kernel_pagetable(struct Minimal_Memory_Map *memory_map,
                                      struct Devices_List *dev_list)
{
    pagetable_t kpage_table = (pagetable_t)kalloc();
    memset(kpage_table, 0, PAGE_SIZE);

    // map kernel text as executable and read-only.
    kvm_map_or_panic(
        kpage_table, memory_map->kernel_start, memory_map->kernel_start,
        (size_t)end_of_text - memory_map->kernel_start, PTE_RO_TEXT);

    // map kernel data and the physical RAM we'll make use of.
    kvm_map_or_panic(kpage_table, (size_t)end_of_text, (size_t)end_of_text,
                     (size_t)memory_map->ram_end - (size_t)end_of_text,
                     PTE_RW_RAM);

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel.
    kvm_map_or_panic(kpage_table, TRAMPOLINE, (size_t)trampoline, PAGE_SIZE,
                     PTE_RO_TEXT);

    // allocate and map a kernel stack for each process.
    init_per_process_kernel_stack(kpage_table);

    // map all found MMIO devices
    for (size_t i = 0; i < dev_list->dev_array_length; ++i)
    {
        struct Found_Device *dev = &(dev_list->dev[i]);
        if (dev->init_parameters.mmu_map_memory)
        {
            for (size_t i = 0; i < DEVICE_MAX_MEM_MAPS; ++i)
            {
                // memory size of 0 means end of the list
                if (dev->init_parameters.mem[i].size == 0) break;

                // printk("mapping found device %s at 0x%zx size: 0x%zx",
                //        dev->driver->dtb_name,
                //        dev->init_parameters.mem[i].start,
                //        dev->init_parameters.mem[i].size);
                // if (dev->init_parameters.mem[i].name)
                //{
                //     printk(" (%s)", dev->init_parameters.mem[i].name);
                // }
                // printk("\n");

                kvm_map_mmio(kpage_table, dev->init_parameters.mem[i].start,
                             PAGE_ROUND_UP(dev->init_parameters.mem[i].size));
            }
        }
    }

    return kpage_table;
}

void kvm_init(struct Minimal_Memory_Map *memory_map,
              struct Devices_List *dev_list)
{
    g_kernel_pagetable = kvm_make_kernel_pagetable(memory_map, dev_list);
}

#if defined(__ARCH_is_32bit)
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
#if defined(__ARCH_is_64bit)
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

size_t uvm_get_physical_addr(pagetable_t pagetable, size_t va,
                             bool *is_writeable)
{
    size_t offset = va % PAGE_SIZE;
    size_t va_page = va - offset;
    size_t pa_page = uvm_get_physical_paddr(pagetable, va_page, is_writeable);
    if (pa_page == 0)
    {
        return 0;
    }
    return pa_page + offset;
}

size_t uvm_get_physical_paddr(pagetable_t pagetable, size_t va,
                              bool *is_writeable)
{
#if defined(__ARCH_is_64bit)
    if (va >= MAXVA)
    {
        return 0;
    }
#endif

    pte_t *pte = vm_walk(pagetable, va, false);
    if (pte == NULL) return 0;
    if ((*pte & PTE_V) == 0) return 0;
    if ((*pte & PTE_U) == 0) return 0;

    // optionally pass out if the page can be written to
    if (is_writeable) *is_writeable = PTE_IS_WRITEABLE(*pte);

    size_t pa = PTE2PA(*pte);
    return pa;
}

int32_t kvm_map_or_panic(pagetable_t k_pagetable, size_t va, size_t pa,
                         size_t size, pte_t perm)
{
    if (kvm_map(k_pagetable, va, pa, size, perm) != 0)
    {
        panic("kvm_map_or_panic failed");
    }
    return 0;
}

int32_t kvm_map(pagetable_t pagetable, size_t va, size_t pa, size_t size,
                pte_t perm)
{
    perm |= PTE_MAP_DEFAULT_FLAGS;

    /*
    // useful debug prints
    printk("mapping (physical 0x%x) to 0x%x - 0x%x (size: %d pages) (", pa, va,
           va + size - 1, size / PAGE_SIZE);
    debug_vm_print_pte_flags(perm);
    printk(")\n");
    */

    if ((va % PAGE_SIZE) != 0)
    {
        panic("kvm_map: va not aligned");
    }

    if ((size % PAGE_SIZE) != 0)
    {
        panic("kvm_map: size not aligned");
    }

    if (size == 0)
    {
        panic("kvm_map: size == 0");
    }

    size_t current_va = va;
    size_t last_va = va + size - PAGE_SIZE;
    while (true)
    {
        pte_t *pte = vm_walk(pagetable, current_va, true);
        if (pte == NULL)
        {
            // out of memory
            return -1;
        }
        if (PTE_IS_IN_USE(*pte) & PTE_V)
        {
            panic("kvm_map: remap");
        }
        *pte = PA2PTE(pa) | perm;
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

    for (size_t page = 0; page < npages; ++page)
    {
        size_t a = va + page * PAGE_SIZE;
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

size_t uvm_alloc_heap(pagetable_t pagetable, size_t start_va, size_t alloc_size,
                      pte_t perm)
{
    size_t end_va = start_va + alloc_size;
    start_va = PAGE_ROUND_UP(start_va);
    size_t n = 0;
    for (size_t va = start_va; va < end_va; va += PAGE_SIZE)
    {
        char *mem = kalloc();
        if (mem == 0)
        {
            uvm_dealloc_heap(pagetable, va, va - start_va);
            return 0;
        }
        n++;
        // All memory that the kernel makes availabe to user apps gets
        // cleared. In a real OS this is a security feature to prevent apps
        // from reading private data previously owned by another app.
        // It is also required for the apps BSS section. As we don't
        // handle BSS sections in a special way, it only works rigth by clearing
        // all memory.
        memset(mem, 0, PAGE_SIZE);
        if (kvm_map(pagetable, va, (size_t)mem, PAGE_SIZE,
                    PTE_USER_RAM | perm) != 0)
        {
            kfree(mem);
            uvm_dealloc_heap(pagetable, va, va - start_va);
            return 0;
        }
    }
    return alloc_size;
}

size_t uvm_dealloc_heap(pagetable_t pagetable, size_t end_va,
                        size_t dealloc_size)
{
    size_t new_end_va = end_va - dealloc_size;
    struct process *proc = get_current();
    if (new_end_va < proc->heap_begin)
    {
        return 0;
    }
    // start deallocating one page up if the page of the first address to clear
    // is still partially used
    size_t start_dealloc_va = PAGE_ROUND_UP(new_end_va);

    size_t npages = (PAGE_ROUND_UP(end_va) - start_dealloc_va) / PAGE_SIZE;

    // note: unmap of 0 pages is fine!
    uvm_unmap(pagetable, start_dealloc_va, npages, true);

    return dealloc_size;
}

int32_t uvm_create_stack(pagetable_t pagetable, char **argv,
                         size_t *stack_low_out, size_t *sp_out)
{
    size_t sp = USER_STACK_HIGH;
    size_t stack_low = uvm_grow_stack(pagetable, USER_STACK_HIGH);
    if (stack_low == 0)
    {
        return -1;
    }

    size_t argc = 0;
    if (argv != NULL)
    {
        // Push argument strings, prepare rest of stack in ustack.
        size_t ustack[MAX_EXEC_ARGS];
        for (argc = 0; argv[argc] && argc < MAX_EXEC_ARGS; argc++)
        {
            // 16-byte aligned space on the stack for the string
            // (sp alignement is a RISC V requirement)
            sp -= strlen(argv[argc]) + 1;
            sp -= sp % 16;
            if (sp < stack_low)
            {
                // stack overflow
                return -1;
            }

            if (uvm_copy_out(pagetable, sp, argv[argc],
                             strlen(argv[argc]) + 1) < 0)
            {
                return -1;
            }
            ustack[argc] = sp;
        }
        if (argc >= MAX_EXEC_ARGS)
        {
            return -1;
        }
        ustack[argc] = 0;

        // push the array of argv[] pointers.
        sp -= (argc + 1) * sizeof(size_t);
        sp -= sp % 16;
        if (sp < stack_low)
        {
            return -1;
        }
        if (uvm_copy_out(pagetable, sp, (char *)ustack,
                         (argc + 1) * sizeof(size_t)) < 0)
        {
            return -1;
        }
    }

    *sp_out = sp;
    *stack_low_out = stack_low;
    return argc;
}

size_t uvm_grow_stack(pagetable_t pagetable, size_t stack_low)
{
    char *mem = kalloc();
    if (mem == NULL)
    {
        return 0;
    }

    memset(mem, 0, PAGE_SIZE);
    size_t new_stack_low = stack_low - PAGE_SIZE;
    if (kvm_map(pagetable, new_stack_low, (size_t)mem, PAGE_SIZE,
                PTE_RW | PTE_U) != 0)
    {
        kfree(mem);
        return 0;
    }

    return new_stack_low;
}

void uvm_free_pagetable(pagetable_t pagetable)
{
    // there are 2^9 => 512 PTEs in a page table on 64 bit RISC V
    // there are 2^10 => 1024 PTEs in a page table on 32 bit RISC V
    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
    {
        pte_t pte = pagetable[i];
        size_t child = PTE2PA(pte);

        if (PTE_IS_VALID_NODE(pte))
        {
            if (PTE_IS_LEAF(pte))
            {
                // a leaf pointing to a mapped page
                kfree((void *)child);
            }
            else
            {
                // this PTE points to a lower-level page table
                uvm_free_pagetable((pagetable_t)child);
            }
        }
        pagetable[i] = 0;
    }
    kfree((void *)pagetable);
}

int32_t uvm_copy(pagetable_t src_page, pagetable_t dst_page, size_t va_start,
                 size_t va_end)
{
    bool fatal_error_happened = false;
    va_start = PAGE_ROUND_DOWN(va_start);

    size_t pages_mapped = 0;
    for (size_t va = va_start; va < va_end; va += PAGE_SIZE)
    {
        pte_t *pte = vm_walk(src_page, va, false);
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
        if (kvm_map(dst_page, va, (size_t)mem, PAGE_SIZE, flags) != 0)
        {
            kfree(mem);
            fatal_error_happened = true;
            break;
        }

        pages_mapped++;
    }

    if (fatal_error_happened)
    {
        // unmap and free the partial copy
        uvm_unmap(dst_page, va_start, pages_mapped, true);
        return -1;
    }

    return 0;
}

void uvm_clear_user_access_bit(pagetable_t pagetable, size_t va)
{
    pte_t *pte = vm_walk(pagetable, va, false);
    if (pte == NULL)
    {
        panic("uvm_clear_user_access_bit");
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
        bool dst_page_is_writeable;
        size_t dst_pa_page_start = uvm_get_physical_paddr(
            pagetable, dst_va_page_start, &dst_page_is_writeable);

        if (dst_pa_page_start == 0 || !dst_page_is_writeable)
        {
            // page not mapped or read-only
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
            uvm_get_physical_paddr(pagetable, src_va_page_start, NULL);
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
            uvm_get_physical_paddr(pagetable, src_va_page_start, NULL);
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

#if defined(DEBUG)
void debug_print_pt_level(pagetable_t pagetable, size_t level,
                          size_t partial_va)
{
    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
    {
        pte_t pte = pagetable[i];
        if (pte & PTE_V)
        {
            for (size_t j = 0; j < PAGE_TABLE_MAX_LEVELS - level; ++j)
            {
                printk("-");
            }
            printk(" %zd: pa: 0x%zx ", i, PTE2PA(pte));
            debug_vm_print_pte_flags(pte);

            size_t va = partial_va | VA_FROM_PAGE_TABLE_INDEX(level, i);
            if (level > 0)
            {
                printk("\n");

                pagetable_t sub_pagetable = (pagetable_t)PTE2PA(pte);
                debug_print_pt_level(sub_pagetable, level - 1, va);
            }
            else
            {
                printk(" - va: 0x%zx\n", va);
            }
        }
        else
        {
            continue;
        }
    }
}

void debug_vm_print_page_table(pagetable_t pagetable)
{
    printk("page table 0x%p\n", pagetable);
    debug_print_pt_level(pagetable, PAGE_TABLE_MAX_LEVELS - 1, 0);
}

size_t debug_vm_get_size_level(pagetable_t pagetable, size_t level)
{
    size_t size = 0;
    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
    {
        pte_t pte = pagetable[i];
        if (pte & PTE_V)
        {
            size++;  // count the page this pte points to

            // don't descent into level 0 as it does not point
            // to additional allocations
            if (level > 1)
            {
                pagetable_t sub_pagetable = (pagetable_t)PTE2PA(pte);
                size += debug_vm_get_size_level(sub_pagetable, level - 1);
            }
        }
    }
    return size;
}

size_t debug_vm_get_size(pagetable_t pagetable)
{
    // +1 to count the page pagetable points to itself.
    return 1 + debug_vm_get_size_level(pagetable, PAGE_TABLE_MAX_LEVELS - 1);
}

#endif  // DEBUG
