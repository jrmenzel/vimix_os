/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <kernel/page.h>
#include <kernel/spinlock.h>
#include <mm/pte.h>
#include <mm/vm.h>

extern pagetable_t g_kernel_pagetable;
extern struct spinlock g_kernel_pagetable_lock;

// Memory map filled in main from a device tree and used to init the free memory
struct Minimal_Memory_Map
{
    size_t ram_start;  // RAM could contain BIOS/bootloader code before the
                       // kernel code starts
    size_t kernel_start;
    size_t kernel_end;  // after the kernel and its data incl BSS
    size_t ram_end;
    size_t initrd_begin;  // 0 if there is no initrd
    size_t initrd_end;    // 0 if there is no initrd
    size_t dtb_file_start;
    size_t dtb_file_end;
};

/// @brief Enables paging with the page table pointed to and sets the ASID.
/// @param addr_of_first_block Pointer to page table.
/// @param asid ASID to use.
void mmu_set_page_table(size_t addr_of_first_block, uint32_t asid);

/// @brief Construct the page table enable register value. It encodes the page
/// table address, the ASID and flags. The format is ARCH specific.
/// @param addr_of_first_block Pointer to page table.
/// @param asid ASID to use.
/// @return The register value for mmu_set_page_table_reg_value()
size_t mmu_make_page_table_reg(size_t addr_of_first_block, uint32_t asid);

/// @brief Sets paging register including all required barriers.
/// Implemented in assembly to share code with the user mode trap vector.
/// @param reg_value Register value to set.
void mmu_set_page_table_reg_value(size_t reg_value);

/// @brief Returns the current page table settings.
/// @return The register value for a future mmu_set_page_table_reg_value()
size_t mmu_get_page_table_reg_value();

/// @brief Extracts the pointer value to the page table.
/// @param reg_value from mmu_get_page_table_reg_value()
/// @return Address of page table
size_t mmu_get_page_table_address(size_t reg_value);

/// @brief Extracts the ASID.
/// @param reg_value from mmu_get_page_table_reg_value()
/// @return ASID
size_t mmu_get_page_table_asid(size_t reg_value);

/// Initialize the one g_kernel_pagetable
void kvm_init(struct Minimal_Memory_Map *memory_map,
              struct Devices_List *dev_list);

/// @brief add a mapping to the page table.
/// only used when booting, does not flush TLB or enable paging. Kernel panic if
/// mapping failed.
/// @param k_pagetable page table to modify
/// @param va virtual address
/// @param pa physical address
/// @param size size in bytes
/// @param perm access permission
int32_t kvm_map_or_panic(pagetable_t k_pagetable, size_t va, size_t pa,
                         size_t size, pte_t perm);

/// @brief Look up the physical address of a page behind a virtual address.
///        Can only be used to look up kernel pages.
/// @param va (kernel) virtual address to look up.
/// @return Physical address or 0 if not mapped.
size_t kvm_get_physical_paddr(size_t va);

/// @brief Create PTEs for virtual addresses starting at va that refer to
/// physical addresses starting at pa. va and size must be page-aligned.
/// @param pagetable kernel page table to modify
/// @param va virtual address
/// @param pa physical address
/// @param size size in bytes
/// @param perm access permission
/// @param allow_super_pages If the mapping can be (partially) done with super
/// pages, those will be allocated (e.g. 2 MB alligned 2 MB blocks on 64 bit).
/// @return Returns 0 on success, -1 if vm_walk() couldn't allocate a needed
/// page-table page.
int32_t vm_map(pagetable_t pagetable, size_t va, size_t pa, size_t size,
               pte_t perm, bool allow_super_pages);

/// @brief Allocate PTEs and physical memory to grow the process heap (and text,
/// data, bss segments at load/execv).
/// [round_up(start_va) to end_va] gets mapped. start_va is round up to the next
/// page (no change if it was page aligned).
/// @param pagetable page table
/// @param start_va start of new allocation
/// @param alloc_size size of allocation in bytes
/// @param perm access permission
/// @return Returns allocated bytes or 0 on error.
size_t uvm_alloc_heap(pagetable_t pagetable, size_t start_va, size_t alloc_size,
                      pte_t perm);

/// @brief Deallocate user pages to bring the process heap down.
///  oldsz and newsz need not be page-aligned, nor does newsz
/// need to be less than oldsz.  oldsz can be larger than the actual
/// process size.
/// @param pagetable Table to dealloc from
/// @param end_va current end of heap
/// @param dealloc_size number of bytes to dealloc
/// @return Returns bytes deallocated
size_t uvm_dealloc_heap(pagetable_t pagetable, size_t end_va,
                        size_t dealloc_size);

/// @brief Create a new user stack and fill it with argv as required by execv.
/// @param pagetable The pagetable to add the stack pages to.
/// @param argv NULL or argv from execv (NULL terminated array of strings)
/// @param stack_low_out Returns where the stack ends, lowest page address
/// @param sp_out Returns the stack pointer
/// @return argc (argv length) on success, -1 on failure
int32_t uvm_create_stack(pagetable_t pagetable, char **argv,
                         size_t *stack_low_out, size_t *sp_out);

/// @brief Grows a user stack by one page
/// @param pagetable The pagetable to add the stack page to.
/// @param stack_low Currently lowest stack page address, new page will get
/// allocated just below.
/// @return now lowest stack address, 0 on failure
size_t uvm_grow_stack(pagetable_t pagetable, size_t stack_low);

/// @brief Given a parent process's page table, copy
/// its memory into a child's page table.
/// Copies both the page table and the
/// physical memory. Copies whole pages.
/// @param src_page page table to copy from
/// @param dst_page page table to copy to
/// @param va_start First address to copy
/// @param va_end Last address to copy
/// @return 0 on success, -1 on failure. Frees any allocated pages on failure.
int32_t uvm_copy(pagetable_t src_page, pagetable_t dst_page, size_t va_start,
                 size_t va_end);

/// @brief Free user memory pages, then free page-table pages.
// void uvm_free(pagetable_t pagetable, size_t sz);

/// @brief Free user memory pages, then free page-table pages.
void uvm_free_pagetable(pagetable_t pagetable);

/// @brief Remove npages of mappings starting from va. The mappings must exist.
/// @param pagetable Pagetable for address translation.
/// @param va Virtual address of start to unmap, must be page-aligned.
/// @param npages Number of pages to unmap.
/// @param do_free Free physical pages if set to true.
void uvm_unmap(pagetable_t pagetable, size_t va, size_t npages, bool do_free);

/// @brief mark a PTE invalid for user access.
/// @param va Virtual address of the page to prevent user access to.
/// Used by execv for the user stack guard page.
void uvm_clear_user_access_bit(pagetable_t pagetable, size_t va);

/// @brief Return PTE in pagetable which maps the given address va. Optionally
/// create any required page-table pages.
/// @param pagetable The page table to look-up in or extend (see alloc)
/// @param va virtual address to look up PTE for
/// @param alloc if true extend page table to map the given address
/// @return PTE or NULL if va is not mapped and alloc == FALSE.
pte_t *vm_walk(pagetable_t pagetable, size_t va, bool alloc);

/// @brief Return PTE in pagetable which maps the given address va. Optionally
/// create any required page-table pages.
/// @param pagetable The page table to look-up in or extend (see alloc)
/// @param va virtual address to look up PTE for
/// @param is_super_page Set to true if the PTE maps a (usually) 2MB region
/// instead of a 4KB page. Must also be true to allow super page creation at
/// alloc.
/// @param alloc if true extend page table to map the given address
/// @return PTE or NULL if va is not mapped and alloc == FALSE.
pte_t *vm_walk2(pagetable_t pagetable, size_t va, bool *is_super_page,
                bool alloc);

/// @brief Look up the physical address behind a virtual address.
///        Can only be used to look up user pages.
///        If only the address of a page is needed (va lowest 12 bits 0) use
///        uvm_get_physical_paddr().
/// @param pagetable Pagetable for address translation.
/// @param va (user) virtual address to look up.
/// @param is_writeable Optional to pass out if the page can be written to.
/// @return Physical address or 0 if not mapped.
size_t uvm_get_physical_addr(pagetable_t pagetable, size_t va,
                             bool *is_writeable);

/// @brief Look up the physical address of a page behind a virtual address.
///        Can only be used to look up user pages.
///        If a lookup of arbitrary addresses is needed, use
///        uvm_get_physical_addr().
/// @param pagetable Pagetable for address translation.
/// @param va (user) virtual address to look up.
/// @param is_writeable Optional to pass out if the page can be written to.
/// @return Physical address or 0 if not mapped.
size_t uvm_get_physical_paddr(pagetable_t pagetable, size_t va,
                              bool *is_writeable);

/// @brief Copy from kernel to user.
/// Copy len bytes from src_pa to dst_va in a given page table.
/// @param pagetable Pagetable for address translation.
/// @param dst_va Destination (user) virtual address.
/// @param src_pa Source (kernel) physical address.
/// @param len Bytes to copy
/// @return 0 on success, -1 on failure.
int32_t uvm_copy_out(pagetable_t pagetable, size_t dst_va, char *src_pa,
                     size_t len);

/// @brief Copy from user to kernel.
/// @param pagetable Pagetable for address translation.
/// @param dst_pa Destination (kernel) physical address.
/// @param src_va Source (user) virtual address.
/// @param len Bytes to copy
/// @return 0 on success, -1 on failure.
int32_t uvm_copy_in(pagetable_t pagetable, char *dst_pa, size_t src_va,
                    size_t len);

/// @brief Copy a null-terminated string from user to kernel.
/// @param pagetable Pagetable for address translation.
/// @param dst_pa Destination (kernel) physical address.
/// @param src_va Source (user) virtual address.
/// @param max Max bytes to copy (unless '\0' was found before)
/// @return 0 on success, -1 on failure.
int32_t uvm_copy_in_str(pagetable_t pagetable, char *dst_pa, size_t src_va,
                        size_t max);

/// @brief Trim unused page table pages from the pagetable. E.g. after a leave
/// was removed from the last level of the pagetable, the second last level
/// might be empty and can be freed.
/// @param pagetable The page table to trim.
/// @param va_start Optional address where a page was unmapped to limit trim
/// search to the path to that va.
/// @return true if any page of the table page was freed.
bool vm_trim_pagetable(pagetable_t pagetable, size_t va_removed);

#if defined(DEBUG)
/// @brief Debug print of the pagetable.
/// @param pagetable Table to print.
void debug_vm_print_page_table(pagetable_t pagetable);

/// @brief Get the size of the pagetable in pages.
/// @param pagetable The page table.
/// @return Number of allocations, size in byte = return value * PAGE_SIZE
size_t debug_vm_get_size(pagetable_t pagetable);

/// @brief Prints the flags from the PTE
/// @param flags ARCH specific flags or a full PTE (addr will be ignored)
void debug_vm_print_pte_flags(size_t flags);

#else
static inline void debug_vm_print_page_table(pagetable_t) {}
static inline size_t debug_vm_get_size(pagetable_t pagetable) { return 0; }
static inline void debug_vm_print_pte_flags(size_t flags) {}
#endif  // DEBUG

/// the va is in the range of a valid user virtual address (starting at 0)
#define VA_IS_IN_RANGE_FOR_USER(va) (va < USER_VA_END)

// TODO: set after remapping
#define KERNEL_VA_START 0

/// the va is in the range of a valid kernel virtual address (ending at
/// FFFFF...)
#define VA_IS_IN_RANGE_FOR_KERNEL(va) (va >= KERNEL_VA_START)

/// va is valid either for the user or the kernel
#define VA_IS_IN_RANGE(va) \
    (VA_IS_IN_RANGE_FOR_USER(va) || VA_IS_IN_RANGE_FOR_KERNEL(va))
