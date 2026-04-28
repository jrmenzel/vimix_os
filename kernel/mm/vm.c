/* SPDX-License-Identifier: MIT */

#include <init/start.h>
#include <kernel/elf.h>
#include <kernel/errno.h>
#include <kernel/fs.h>
#include <kernel/kernel.h>
#include <kernel/pgtable.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <mm/arch_vm.h>
#include <mm/kalloc.h>
#include <mm/kernel_memory.h>
#include <mm/memlayout.h>
#include <mm/mm.h>
#include <mm/page_table.h>
#include <mm/vm.h>

size_t mmu_set_page_table(pagetable_t pgtable, uint32_t asid)
{
    size_t reg_value = mmu_make_page_table_reg((size_t)pgtable, asid);
    mmu_set_page_table_reg_value(reg_value);
    return reg_value;
}

syserr_t kvm_apply_mapping(struct Page_Table *kpagetable)
{
    syserr_t err = page_table_apply_mapping(kpagetable);
    if (err < 0) return err;

    mmu_set_kernel_page_table(kpagetable);

    return 0;
}

#if defined(__ARCH_32BIT)
const size_t MAX_PTES_PER_PAGE_TABLE = 1024;
#else
const size_t MAX_PTES_PER_PAGE_TABLE = 512;
#endif

static inline bool vm_is_valid_page_pointer_pa(size_t pa)
{
    if (pa == 0) return false;
    if ((pa % PAGE_SIZE) != 0) return false;

    return addr_is_in_ram_pa(pa);
}

static inline bool vm_is_valid_page_pointer_kva(size_t kva)
{
    if (kva == 0) return false;
    if ((kva % PAGE_SIZE) != 0) return false;

    return addr_is_in_ram_kva(kva);
}

pte_t *vm_walk2(struct Page_Table *pagetable, size_t va, bool *is_super_page,
                bool alloc)
{
    DEBUG_ASSERT_CPU_HOLDS_LOCK(&pagetable->lock);
    if (!VA_IS_IN_RANGE(va))
    {
        printk("vm_walk: virtual address 0x%zx is not supported\n", va);
        panic("vm_walk: virtual address is out of range");
    }

    if (alloc && is_super_page == NULL)
    {
        panic(
            "vm_walk: super page flag must be set when potentially allocating "
            "a mapping");
    }

    pagetable_t pgtable = pagetable->root;

    for (size_t level = (PAGE_TABLE_MAX_LEVELS - 1); level > 0; level--)
    {
        pte_t *pte = &pgtable[PAGE_TABLE_INDEX(level, va)];

        if (!PTE_IS_VALID_NODE(*pte))
        {
            // empty -> alloc or fail
            if (alloc)
            {
                if (level == 1 && *is_super_page)
                {
                    return pte;
                }
                pgtable = (pagetable_t)alloc_page(ALLOC_FLAG_ZERO_MEMORY);
                if (pgtable == NULL) return NULL;

                size_t pagetable_pa = virt_to_phys((size_t)pgtable);
                *pte = PTE_BUILD(pagetable_pa, 0);
                PTE_MAKE_VALID_TABLE(*pte);
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            // a valid / already in use PTE
            if (PTE_IS_LEAF(*pte))
            {
                if (is_super_page) *is_super_page = true;
                return pte;
            }
            // else it's pointing to the next level of the tree:
            size_t next_level_pa = PTE_GET_PA(*pte);
            size_t next_level_va = phys_to_virt(next_level_pa);
            pgtable = (pagetable_t)next_level_va;
        }
    }
    return &pgtable[PAGE_TABLE_INDEX(0, va)];
}

pte_t *vm_walk(struct Page_Table *pagetable, size_t va, bool alloc)
{
    bool super = false;
    return vm_walk2(pagetable, va, &super, alloc);
}

size_t uvm_get_physical_addr(struct Page_Table *pagetable, size_t va,
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

size_t uvm_get_physical_paddr(struct Page_Table *pagetable, size_t va,
                              bool *is_writeable)
{
    if (!VA_IS_IN_RANGE_FOR_USER(va))
    {
        return 0;
    }

    pte_t *pte = vm_walk2(pagetable, va, NULL, false);
    if (pte == NULL) return 0;
    if (PTE_IS_VALID_USER(*pte) == false) return 0;

    // optionally pass out if the page can be written to
    if (is_writeable) *is_writeable = PTE_IS_WRITEABLE(*pte);

    size_t pa = PTE_GET_PA(*pte);
    return pa;
}

size_t kvm_get_physical_paddr(size_t va)
{
    pte_t *pte = vm_walk2(g_kernel_pagetable, va, NULL, false);
    if (pte == NULL) return 0;
    if (PTE_IS_VALID_NODE(*pte) == false) return 0;

    size_t pa = PTE_GET_PA(*pte);
    return pa;
}

int32_t vm_map(struct Page_Table *pagetable, size_t va, size_t pa, size_t size,
               pte_t perm, bool allow_super_pages)
{
    DEBUG_EXTRA_PANIC(
        allow_super_pages == false,
        "super pages are disabled, not all code is aware, e.g. unmapping code");

    perm |= PTE_MAP_DEFAULT_FLAGS;

    if ((va % PAGE_SIZE) != 0)
    {
        panic("vm_map: va not aligned");
    }

    if ((size % PAGE_SIZE) != 0)
    {
        panic("vm_map: size not aligned");
    }

    if (size == 0)
    {
        panic("vm_map: size == 0");
    }

    size_t current_va = va;
    size_t current_pa = pa;
    size_t remaining_size = size;
    while (remaining_size > 0)
    {
        bool alloc_super_page = false;
        size_t bytes_mapped = PAGE_SIZE;
        if (allow_super_pages && (current_va % MEGA_PAGE_SIZE == 0) &&
            (current_pa % MEGA_PAGE_SIZE == 0) &&
            (remaining_size >= MEGA_PAGE_SIZE))
        {
            alloc_super_page = true;
            bytes_mapped = MEGA_PAGE_SIZE;
        }
        pte_t *pte = vm_walk2(pagetable, current_va, &alloc_super_page, true);
        if (pte == NULL)
        {
            // out of memory
            return -1;
        }

        pte_t new_value = PTE_BUILD(current_pa, perm);
        PTE_MAKE_VALID_LEAF(new_value);

        if (*pte == new_value)
        {
            // Ignore remapping if the target address and the flags match
            // the new mapping. This can happen when the MMIO devices are
            // mapped and multiple devices share the same pages.
        }
        else
        {
            if (PTE_IS_VALID_NODE(*pte))
            {
                printk("remapping a node PTE\n");
                printk(
                    "mapping (physical 0x%zx) to 0x%zx - 0x%zx (size: %d "
                    "pages) (",
                    pa, va, va + size - 1, (int)(size / PAGE_SIZE));
                debug_vm_print_pte_flags(perm);

                printk(")\nmapped:\n");
                printk(" pa: 0x%zx ", PTE_GET_PA(*pte));
                debug_vm_print_pte_flags(*pte);
                printk("\n");

                panic("vm_map: remap");
            }

            *pte = new_value;
        }

        current_va += bytes_mapped;
        current_pa += bytes_mapped;
        remaining_size -= bytes_mapped;
    }
    return 0;
}

void vm_unmap(struct Page_Table *pagetable, size_t va, size_t npages,
              bool do_free)
{
    if ((va % PAGE_SIZE) != 0)
    {
        panic("vm_unmap: not aligned");
    }

    for (size_t page = 0; page < npages; ++page)
    {
        size_t a = va + page * PAGE_SIZE;
        pte_t *pte = vm_walk(pagetable, a, false);
        if (pte == NULL)
        {
            panic("vm_unmap: vm_walk");
        }
        if (PTE_IS_VALID_NODE(*pte) == false)
        {
            panic("vm_unmap: not mapped");
        }
        if (PTE_IS_LEAF(*pte) == false)
        {
            panic("vm_unmap: not a leaf");
        }
        if (do_free)
        {
            size_t pa = PTE_GET_PA(*pte);
            free_page((void *)phys_to_virt(pa));
        }
        *pte = 0;
    }
}

size_t uvm_alloc_heap(struct Page_Table *pagetable, size_t start_va,
                      size_t alloc_size, enum MM_Region_Type map_type)
{
    spin_lock(&pagetable->lock);
    size_t end_va = start_va + alloc_size;
    start_va = PAGE_ROUND_UP(start_va);
    size_t va = start_va;
    for (; va < end_va; va += PAGE_SIZE)
    {
        // All memory that the kernel makes availabe to user apps gets
        // cleared. In a real OS this is a security feature to prevent apps
        // from reading private data previously owned by another app.
        // It is also required for the apps BSS section. As we don't
        // handle BSS sections in a special way, it only works rigth by clearing
        // all memory.
        char *mem = (char *)alloc_page(ALLOC_FLAG_ZERO_MEMORY);
        if (mem == NULL)
        {
            break;
        }

        struct MM_Region *region = mm_region_alloc_init(
            virt_to_phys((size_t)mem), va, PAGE_SIZE, map_type);
        if (region == NULL)
        {
            kfree(mem);
            break;
        }
        memory_map_add_single_region(&pagetable->memory_map, region);
    }
    if (va < end_va)
    {
        // out of memory: clean up and return failure
        // we can assume no allocations from the va loop, but
        // must leanup everything from the earlier loops
        page_table_unmap_partial_mappings(pagetable);
        spin_unlock(&pagetable->lock);
        return 0;
    }

    // apply mapping, failure will clean up in memory map
    syserr_t err = page_table_apply_mapping(pagetable);
    spin_unlock(&pagetable->lock);
    if (err < 0)
    {
        return 0;
    }

    return alloc_size;
}

size_t uvm_dealloc_heap(struct Page_Table *pagetable, size_t end_va,
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

    if (npages > 0)
    {
        spin_lock(&pagetable->lock);
        page_table_unmap_range(pagetable, start_dealloc_va, npages * PAGE_SIZE);
        spin_unlock(&pagetable->lock);
    }

    return dealloc_size;
}

int32_t uvm_create_stack(struct Page_Table *pagetable, char **argv,
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

size_t uvm_grow_stack(struct Page_Table *pagetable, size_t stack_low)
{
    char *mem = alloc_page(ALLOC_FLAG_ZERO_MEMORY);
    if (mem == NULL)
    {
        return 0;
    }

    size_t new_stack_low = stack_low - PAGE_SIZE;

    struct MM_Region *region =
        mm_region_alloc_init(virt_to_phys((size_t)mem), new_stack_low,
                             PAGE_SIZE, MM_REGION_USER_STACK);
    if (region == NULL)
    {
        free_page(mem);
        return 0;
    }
    spin_lock(&pagetable->lock);
    memory_map_add_single_region(&pagetable->memory_map, region);

    if (page_table_apply_mapping(pagetable) != 0)
    {
        spin_unlock(&pagetable->lock);
        return 0;
    }

    spin_unlock(&pagetable->lock);
    return new_stack_low;
}

void vm_free_pgtable(pagetable_t pgtable)
{
    if (pgtable == NULL)
    {
        // nothing to do
        return;
    }
    if (!vm_is_valid_page_pointer_kva((size_t)pgtable))
    {
        printk("vm_free_pgtable: invalid pagetable pointer 0x%zx\n",
               (size_t)pgtable);
        panic("vm_free_pgtable: invalid pagetable pointer");
    }

    // there are 2^9 => 512 PTEs in a page table on 64 bit RISC V
    // there are 2^10 => 1024 PTEs in a page table on 32 bit RISC V
    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
    {
        pte_t pte = pgtable[i];
        size_t child = PTE_GET_PA(pte);

        if (PTE_IS_VALID_NODE(pte))
        {
            if (!vm_is_valid_page_pointer_pa(child))
            {
                printk(
                    "vm_free_pgtable: skipping invalid child pointer "
                    "0x%zx (pte=0x%zx, idx=%zu)\n",
                    child, (size_t)pte, i);
                pgtable[i] = 0;
                continue;
            }

            if (PTE_IS_LEAF(pte))
            {
                //     // a leaf pointing to a mapped page
                //     free_page((void *)phys_to_virt(child));
            }
            else
            {
                // this PTE points to a lower-level page table
                vm_free_pgtable((pagetable_t)phys_to_virt(child));
            }
        }
        pgtable[i] = 0;
    }
    free_page((void *)pgtable);
}

int32_t uvm_copy_out(struct Page_Table *pagetable, size_t dst_va, char *src_pa,
                     size_t len)
{
    spin_lock(&pagetable->lock);

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
            spin_unlock(&pagetable->lock);
            return -1;
        }

        size_t dst_offset_in_page = dst_va - dst_va_page_start;
        size_t n = PAGE_SIZE - dst_offset_in_page;
        if (n > len)
        {
            n = len;
        }
        memmove((void *)(phys_to_virt(dst_pa_page_start) + dst_offset_in_page),
                src_pa, n);

        len -= n;
        src_pa += n;
        dst_va = dst_va_page_start + PAGE_SIZE;
    }

    spin_unlock(&pagetable->lock);
    return 0;
}

int32_t uvm_copy_in(struct Page_Table *pagetable, char *dst_pa, size_t src_va,
                    size_t len)
{
    spin_lock(&pagetable->lock);
    while (len > 0)
    {
        // copy up to one page each loop

        size_t src_va_page_start = PAGE_ROUND_DOWN(src_va);
        size_t src_pa_page_start =
            uvm_get_physical_paddr(pagetable, src_va_page_start, NULL);
        if (src_pa_page_start == 0)
        {
            spin_unlock(&pagetable->lock);
            return -1;
        }

        size_t src_offset_in_page = src_va - src_va_page_start;
        size_t n = PAGE_SIZE - src_offset_in_page;
        if (n > len)
        {
            n = len;
        }
        memmove(dst_pa,
                (void *)(phys_to_virt(src_pa_page_start) + src_offset_in_page),
                n);

        len -= n;
        dst_pa += n;
        src_va = src_va_page_start + PAGE_SIZE;
    }

    spin_unlock(&pagetable->lock);
    return 0;
}

int32_t uvm_copy_in_str(struct Page_Table *pagetable, char *dst_pa,
                        size_t src_va, size_t max)
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

        char *src_pa =
            (char *)(phys_to_virt(src_pa_page_start) + src_offset_in_page);
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

bool vm_trim_pagetable(struct Page_Table *pagetable, size_t va_removed)
{
    // find the page of the pagetable that mapped va_removed
    pagetable_t parent_of_va_removed = pagetable->root;
    pte_t *pte_of_parent_of_va_removed = NULL;
    for (size_t level = PAGE_TABLE_MAX_LEVELS - 1; level > 0; level--)
    {
        size_t index = PAGE_TABLE_INDEX(level, va_removed);
        pte_t *pte = &parent_of_va_removed[index];

        if (!PTE_IS_VALID_NODE(*pte))
        {
            // the path to va_removed does not exist -> nothing to free
            return false;
        }
        else if (PTE_IS_LEAF(*pte))
        {
            // a leaf pointing to a mapped page -> nothing to free
            return false;
        }

        // this PTE points to a lower-level page table
        pte_of_parent_of_va_removed = pte;
        parent_of_va_removed = (pagetable_t)phys_to_virt(PTE_GET_PA(*pte));
    }

    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
    {
        pte_t pte = parent_of_va_removed[i];
        if (PTE_IS_VALID_NODE(pte))
        {
            // found a valid entry -> cannot free this page table
            return false;
        }
    }

    kfree(parent_of_va_removed);
    *pte_of_parent_of_va_removed = 0;

    return true;
}

size_t g_next_free_mmio_va = MMIO_BASE;

size_t vm_get_mmio_offset(size_t new_mmio_start_pa, size_t new_mmio_size)
{
    size_t va = g_next_free_mmio_va;
    g_next_free_mmio_va += new_mmio_size + PAGE_SIZE;  // padding
    return va - new_mmio_start_pa;
}

#if defined(DEBUG)
static inline size_t debug_va_sign_extend(size_t va)
{
#if defined(__ARCH_64BIT)
    // Rebuild canonical Sv39 addresses for debug output.
    // PAGE_SHIFT + PAGE_TABLE_MAX_LEVELS * PT_IDX_BITS = 39 on RV64.
    const size_t used_va_bits =
        PAGE_SHIFT + PAGE_TABLE_MAX_LEVELS * PT_IDX_BITS;
    const size_t sign_bit = used_va_bits - 1;
    if (va & (1UL << sign_bit))
    {
        va |= (~0UL) << used_va_bits;
    }
#endif
    return va;
}

void debug_print_pt_level(pagetable_t pagetable, ssize_t level,
                          size_t partial_va)
{
    if ((level < 0) || (level >= PAGE_TABLE_MAX_LEVELS))
    {
        printk("ERROR, malformatted page table\n");
        return;
    }

    if (pagetable == NULL)
    {
        printk("ERROR, null pagetable pointer\n");
        return;
    }

    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
    {
        pte_t pte = pagetable[i];
        if (PTE_IS_VALID_NODE(pte))
        {
            for (size_t j = 0; j < PAGE_TABLE_MAX_LEVELS - level; ++j)
            {
                printk("-");
            }
            printk(" %zd: pa: 0x%zx ", i, PTE_GET_PA(pte));
            debug_vm_print_pte_flags(pte);

            size_t va = partial_va | VA_FROM_PAGE_TABLE_INDEX(level, i);
            va = debug_va_sign_extend(va);
            if (PTE_IS_LEAF(pte))
            {
                printk(" - va: 0x%zx ", va);

                if (level == 0)
                {
                    printk("(4 KB page)\n");
                }
                else if (level == 1)
                {
                    printk("(%zd MB super page)\n",
                           (size_t)MEGA_PAGE_SIZE / (1024 * 1024));
                }
                else
                {
                    printk("(unexpected leaf)\n");
                }
            }
            else
            {
                printk("\n");

                pagetable_t sub_pagetable_pa = (pagetable_t)PTE_GET_PA(pte);
                pagetable_t sub_pagetable =
                    (pagetable_t)phys_to_virt((size_t)sub_pagetable_pa);
                debug_print_pt_level(sub_pagetable, level - 1, va);
            }
        }
        else
        {
            continue;
        }
    }
}

void debug_vm_print_pgtable(pagetable_t pgtable)
{
    size_t reg_value = mmu_make_page_table_reg((size_t)pgtable, 0);
    printk("page table 0x%p (reg. value: 0x%zx)\n", pgtable, reg_value);
    debug_print_pt_level(pgtable, PAGE_TABLE_MAX_LEVELS - 1, 0);
}

void debug_vm_print_page_table(struct Page_Table *pagetable)
{
    debug_vm_print_pgtable(pagetable->root);

    debug_print_memory_map(&pagetable->memory_map);
}

size_t debug_vm_get_size_level(pagetable_t pgtable, size_t level)
{
    size_t size = 0;
    for (size_t i = 0; i < MAX_PTES_PER_PAGE_TABLE; i++)
    {
        pte_t pte = pgtable[i];
        if (PTE_IS_VALID_NODE(pte))
        {
            size++;  // count the page this pte points to

            // don't descent into level 0 as it does not point
            // to additional allocations
            if (level > 1)
            {
                pagetable_t sub_pgtable_pa = (pagetable_t)PTE_GET_PA(pte);
                pagetable_t sub_pgtable =
                    (pagetable_t)phys_to_virt((size_t)sub_pgtable_pa);
                size += debug_vm_get_size_level(sub_pgtable, level - 1);
            }
        }
    }
    return size;
}

size_t debug_vm_get_size(pagetable_t pgtable)
{
    // +1 to count the page pagetable points to itself.
    return 1 + debug_vm_get_size_level(pgtable, PAGE_TABLE_MAX_LEVELS - 1);
}

void debug_vm_print_pte_flags(size_t flags)
{
    printk("%c", PTE_IS_VALID_NODE(flags) ? 'v' : '_');
    printk("%c", PTE_IS_LEAF(flags) ? 'p' : 't');
    printk("-");
    printk("%c", PTE_IS_USER_ACCESSIBLE(flags) ? 'u' : 'k');
    printk("%c%c%c", PTE_IS_READABLE(flags) ? 'r' : '_',
           PTE_IS_WRITEABLE(flags) ? 'w' : '_',
           PTE_IS_EXECUTABLE(flags) ? 'x' : '_');

    printk("-");
    printk("%c", PTE_WAS_ACCESSED(flags) ? 'a' : '_');
    printk("%c", PTE_IS_GLOBAL(flags) ? 'g' : '_');
    printk("-");
    DEBUG_VM_PRINT_ARCH_PTE_FLAGS(flags);
}

#endif  // DEBUG
