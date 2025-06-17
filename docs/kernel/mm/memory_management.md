# Memory Management


The kernel manages free pages (normally `4KB`) with a buddy allocator in blocks of 1 page, 2 pages, 4, 8 etc.

## Allocate memory in kernel

### One or more pages

Allocate N pages (with N being a power of 2 up to a limit defined by `PAGE_ALLOC_MAX_ORDER` as N = 2 ^ `order`) with `alloc_pages()` (or `alloc_page()` for just one). Free them with `free_pages()` / `free_page()`. Note that if more than one page is allocated, the caller must remember the amount requested and provide the same value to `free_pages()`.

The allocation is optionally zeroed based on the passed flags.

Per supported size of the buddy allocator, one linked list of free blocks is maintained. The pointers for the linked list are stored in the free pages, so no memory is wasted.

This has the following limitations:
- Even if only a few bytes are needed, each allocation will use up at least one full `4KB` page.


## Virtual Memory

Supports Sv39 on 64-bit and Sv32 on 32-bit [RISC V](../../riscv/RISCV.md).

One page table fits exactly into one page of 4Kb:
**64-bit:**
Page table: `pagetable_t`. An array of 512 `pagetable_element` which are just 64-bit ints.

**32-bit:**
Page table: `pagetable_t`. An array of 1024 `pagetable_element` which are just 32-bit ints.


All harts use the same kernel page table:
- all memory is mapped to the physical location
	- 2MB blocks can be mapped as one super page
- [devices](../devices/devices.md) are mapped
- the trampoline function gets mapped to the highest address
See [memory_map_kernel](memory_map_kernel.md) for details.


## User Space Allocations

- one large area of consecutive `4KB` allocations per process
- in virtual memory space, see [memory_map_process](memory_map_process.md)

User space applications can only increase or decrease their memory heap with the [sbrk](../syscalls/sbrk.md) system call. The C standard library will manage this heap and provide convenient `malloc()` and `free()` calls.


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](interrupts.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
