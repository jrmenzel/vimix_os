# Memory Management


The kernel manages free [pages](page.md) with a buddy allocator in blocks of a power-of-2 number of pages (1 page, 2,4,8, etc). Blocks can be requested with `alloc_pages()`.

Smaller allocations can be requested with `kmalloc()`, it uses a number of slab allocators internally which get their memory in full pages from `alloc_pages()`.

## Allocate memory in kernel

### Allocate less than one page of memory

To allocate memory less than one [page](page.md) call `kmalloc()` (free with `kfree()`). Internally it uses a number of slab allocators organized in caches for objects of different sizes. Those are all power-of-2 sizes from `MIN_SLAB_SIZE` to 1/4 of the page size plus a cache for objects up to 1280 bytes. The last one is used by the [block io cache](../file_system/block_io.md). If an allocation is between the largest value supported by the slab allocators and the page size, a full page will get allocated by calling `alloc_pages()`.

As most slabs manage power-of-2 allocation sizes (e.g. 32 byte, 64 byte etc.), so most allocations will use a power-of-2 value of bytes of memory. 

All memory up to one page can be freed with `kfree()` without knowing the allocation size. If the freed pointer is already page aligned, it gets freed via `free_page()`, if it is not aligned, if is an object from a slab allocator. In that case rounding down the pointer to the page boundary will give the responsible slab which is then called with `kmem_slab_free()`.

### One or more pages

Allocate N pages (with N being a power of 2 up to a limit defined by `PAGE_ALLOC_MAX_ORDER` as N = 2 ^ `order`) with `alloc_pages()` (or `alloc_page()` for just one). Free them with `free_pages()` / `free_page()`. Note that if more than one page is allocated, the caller must remember the amount requested and provide the same value to `free_pages()`.

The allocation is optionally zeroed based on the passed flags.

Per supported size of the buddy allocator, one linked list of free blocks is maintained. The pointers for the linked list are stored in the free pages, so no memory is wasted.

This has the following limitations:
- Even if only a few bytes are needed, each allocation will use up at least one full [page](page.md).
- Allocation of multiple pages can fail if there is no continuous memory region free of that size. This can happen even if the total amount of free memory is less than requested.

### Lists

Dynamically growing and shrinking lists of objects are manages by a double linked list (`struct list_head` from `kernel/list.h`).


## Virtual Memory

Supports Sv39 on 64-bit and Sv32 on 32-bit [RISC V](../../riscv/RISCV.md).

One page table fits exactly into one [page](page.md) of `4KB`:
**64-bit:**
Page table: `pagetable_t`. An array of 512 `pagetable_element` which are just 64-bit ints.

**32-bit:**
Page table: `pagetable_t`. An array of 1024 `pagetable_element` which are just 32-bit ints.


All harts use the same kernel page table:
- all memory is mapped to the physical location
	- 2 MB blocks can be mapped as one super page
- [devices](../devices/devices.md) are mapped
- the trampoline function gets mapped to the highest address
See [memory_map_kernel](memory_map_kernel.md) for details.


## User Space Allocations

- one large area of consecutive [page](page.md) sized allocations per process
- in virtual memory space, see [memory_map_process](memory_map_process.md)

User space applications can only increase or decrease their memory heap with the [sbrk](../syscalls/sbrk.md) system call. The C standard library will manage this heap and provide convenient `malloc()` and `free()` calls.


---
**Overview:** [kernel](../kernel.md)

**Subsystems:** [interrupts](interrupts.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)

**Related:** [kernel memory map](memory_map_kernel.md) | [process memory map](memory_map_process.md) | [page](page.md)
