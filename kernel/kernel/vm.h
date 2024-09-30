/* SPDX-License-Identifier: MIT */
#pragma once

#include <drivers/devices_list.h>
#include <kernel/kernel.h>
#include <mm/mm.h>
#include <mm/vm.h>

/// kalloc returns one page of 4KB, so a page used as a pagetable_t
/// will have 4KB/sizeof(size_t) = 512 entries in 64bit / 1024 entries in 32bit
typedef size_t pte_t;

/// @brief in the end a pagetable_t is a "size_t tagetable[512]" (64 bit)
/// / "size_t tagetable[1024]" (32 bit)
typedef pte_t *pagetable_t;
static const pagetable_t INVALID_PAGETABLE_T = NULL;

// Memory map filled in main from a device tree and used to init the free memory
struct Minimal_Memory_Map
{
    size_t ram_start;  // RAM could contain SBI before the kernel code starts
    size_t kernel_start;
    size_t kernel_end;  // after the kernel and its data incl BSS
    size_t ram_end;
    size_t initrd_begin;  // 0 if there is no initrd
    size_t initrd_end;    // 0 if there is no initrd
    size_t dtb_file_start;
    size_t dtb_file_end;
};

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
                         size_t size, int32_t perm);

/// @brief Create PTEs for virtual addresses starting at va that refer to
/// physical addresses starting at pa. va and size might not
/// be page-aligned. Returns 0 on success, -1 if vm_walk() couldn't
/// allocate a needed page-table page.
/// @param pagetable page table to modify
/// @param va virtual address
/// @param pa physical address
/// @param size size in bytes
/// @param perm access permission
int32_t kvm_map(pagetable_t pagetable, size_t va, size_t pa, size_t size,
                int32_t perm);

/// @brief create an empty user page table.
/// @return new pagetable or NULL if out of memory.
pagetable_t uvm_create();

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
                      int32_t perm);

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
pte_t *vm_walk(pagetable_t pagetable, size_t va, bool alloc);

/// @brief Look up the physical address behind a virtual address.
///        Can only be used to look up user pages.
/// @param pagetable Pagetable for address translation.
/// @param va (user) virtual address to look up.
/// @param is_writeable Optional to pass out if the page can be written to.
/// @return Physical address or 0 if not mapped.
size_t uvm_get_physical_addr(pagetable_t pagetable, size_t va,
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

static inline void debug_vm_print_pte_flags(size_t flags)
{
    printk("%c%c%c%c%c%c%c%c", (flags & PTE_V) ? 'v' : '_',
           (flags & PTE_R) ? 'r' : '_', (flags & PTE_W) ? 'w' : '_',
           (flags & PTE_X) ? 'x' : '_', (flags & PTE_U) ? 'u' : '_',
           (flags & PTE_G) ? 'g' : '_', (flags & PTE_A) ? 'a' : '_',
           (flags & PTE_D) ? 'd' : '_');
}

#if defined(DEBUG)
/// @brief Debug print of the pagetable.
/// @param pagetable Table to print.
void debug_vm_print_page_table(pagetable_t pagetable);

/// @brief Get the size of the pagetable in pages.
/// @param pagetable The page table.
/// @return Number of allocations, size in byte = return value * PAGE_SIZE
size_t debug_vm_get_size(pagetable_t pagetable);
#else
static inline void debug_vm_print_page_table(pagetable_t) {}
static inline size_t debug_vm_get_size(pagetable_t pagetable) { return 0; }
#endif  // DEBUG
