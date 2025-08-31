# Memory Map

All kernel threads share the same memory map.
For the [user space](../../userspace/userspace.md) [processes](../processes/processes.md) memory maps, see [memory_map_process](memory_map_process.md).


## Kernel 32-bit


### Special pages mapped to fixed high locations

| VA start  | VA end    | alias               | mapped from       | Permissions |
| --------- | --------- | ------------------- | ----------------- | ----------- |
| FFFF F000 | FFFF FFFF | TRAMPOLINE          | char trampoline[] | R, X        |
| FFFF E000 | FFFF EFFF |                     |                   |             |
| FFFF D000 | FFFF DFFF | proc 0 kernel stack | kalloc()          | R, W        |
| FFFF C000 | FFFF CFFF | (empty guard)       |                   |             |
| FFFF B000 | FFFF BFFF | proc 1 kernel stack | kalloc()          | R, W        |
| FFFF A000 | FFFF AFFF | (empty guard)       |                   |             |
| FFFF 9000 | FFFF 9FFF | proc 2 kernel stack | kalloc()          | R, W        |
|           |           | ... to proc max     |                   |             |

The kernel stacks of the [processes](../processes/processes.md) are only allocated and mapped if the process exists.

### RAM (bare metal)

On bare-metal qemu the RAM starts at 0x8000.0000:

| VA start    | VA end      | alias         | mapped from | Permissions |
| ----------- | ----------- | ------------- | ----------- | ----------- |
| end_of_text | 87FF FFFF   | ~128 MB RAM   | VA = PA     | R, W        |
| 8000 0000   | end_of_text | kernel binary | VA = PA     | R, X        |

### RAM (SBI)

On [SBI](../../riscv/SBI.md) enabled qemu the usable RAM starts at 0x8020.0000:

| VA start    | VA end      | alias         | mapped from | Permissions |
| ----------- | ----------- | ------------- | ----------- | ----------- |
| end_of_text | 87FF FFFF   | ~128 MB RAM   | VA = PA     | R, W        |
| 8020 0000   | end_of_text | kernel binary | VA = PA     | R, X        |

### Memory Mapped IO Devices

| VA start  | VA end    | alias                         | mapped from | Permissions |
| --------- | --------- | ----------------------------- | ----------- | ----------- |
| 1000 2000 | 7FFF FFFF | unused                        |             |             |
| 1000 1000 | 1000 1FFF | VIRTIO disk                   | VA = PA     | R, W        |
| 1000 0000 | 1000 0FFF | UART0                         | VA = PA     | R, W        |
|           |           | unused                        |             |             |
| 0C00 0000 | 0FFF FFFF | [PLIC](../../riscv/PLIC.md)   | VA = PA     | R, W        |
|           |           | unused                        |             |             |
| 0200 0000 | 0200 FFFF | [CLINT](../../riscv/CLINT.md) | VA = PA     | R, W        |
|           |           | unused                        |             |             |
| 0010 1000 | 0010 1FFF | RTC                           | VA = PA     | R, W        |
| 0010 0000 | 0010 0FFF | VIRTIO test                   | VA = PA     | R, W        |


- `kalloc()`: One page provided by kalloc() mapped.
- `VA = PA`: the virtual address matches the physical address


### Kernel 64-bit

Same as 32-bit, except that virtual memory goes up to 0x40.0000.0000 so all special pages are moved up. sv39 supports a max virtual memory address of 2^39 which is 0x80.0000.0000, but VIMIX does not use the highest bit to avoid bit extending addresses.


---
**Overview:** [memory management](memory_management.md)

**Related:** [kernel memory map](memory_map_kernel.md) | [process memory map](memory_map_process.md) | [page](page.md)
