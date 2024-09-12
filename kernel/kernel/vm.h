/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <mm/vm.h>

/// kalloc returns one page of 4KB, so a page used as a pagetable_t
/// will have 4KB/sizeof(size_t) = 512 entries in 64bit / 1024 entries in 32bit
typedef size_t pte_t;

/// @brief in the end a pagetable_t is a "size_t tagetable[512]" (64 bit)
/// / "size_t tagetable[1024]" (32 bit)
typedef pte_t *pagetable_t;
static const pagetable_t INVALID_PAGETABLE_T = NULL;

/// Initialize the one g_kernel_pagetable
void kvm_init(char *end_of_memory);

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

/// @brief Allocate PTEs and physical memory to grow process from oldsz to
/// newsz, which need not be page aligned.
/// @param pagetable page table
/// @param oldsz old size in bytes
/// @param newsz new size in bytes
/// @param xperm access permission
/// @return Returns new size or 0 on error.
size_t uvm_alloc(pagetable_t pagetable, size_t oldsz, size_t newsz,
                 int32_t perm);

/// @brief Deallocate user pages to bring the process size from oldsz to
/// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
/// need to be less than oldsz.  oldsz can be larger than the actual
/// process size.
/// @param pagetable Table to dealloc
/// @param oldsz old size in bytes
/// @param newsz new size in bytes
/// @return Returns the new process size.
size_t uvm_dealloc(pagetable_t pagetable, size_t oldsz, size_t newsz);

/// @brief Given a parent process's page table, copy
/// its memory into a child's page table.
/// Copies both the page table and the
/// physical memory.
/// @param old page table to copy from
/// @param new page table to copy to
/// @param sz size in bytes
/// @return 0 on success, -1 on failure. Frees any allocated pages on failure.
int32_t uvm_copy(pagetable_t old, pagetable_t new, size_t sz);

/// @brief Free user memory pages, then free page-table pages.
void uvm_free(pagetable_t pagetable, size_t sz);

/// @brief Remove npages of mappings starting from va. The mappings must exist.
/// @param pagetable Pagetable for address translation.
/// @param va Virtual address of start to unmap, must be page-aligned.
/// @param npages Number of pages to unmap.
/// @param do_free Free physical pages if set to true.
void uvm_unmap(pagetable_t pagetable, size_t va, size_t npages, bool do_free);

/// @brief mark a PTE invalid for user access.
/// Used by execv for the user stack guard page.
void uvm_clear(pagetable_t pagetable, size_t va);

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
