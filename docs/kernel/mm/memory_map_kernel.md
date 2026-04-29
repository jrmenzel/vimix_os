# Memory Map

All kernel threads share the same memory map.
For the [user space](../../userspace/userspace.md) [processes](../processes/processes.md) memory maps, see [memory_map_process](memory_map_process.md).

Below is a memory map of the kernel:
```
PA 0x0000000080200000, VA 0x0000000080200000, size      4kb, kernel entry at PA
PA 0x000000008022f000, VA 0x0000003ffffff000, size      4kb, kernel text
PA 0x0000000000000000, VA 0xffffffc000000000, size   2048kb, reserved, unmapped
PA 0x0000000080200000, VA 0xffffffc000200000, size    192kb, kernel text
PA 0x0000000080230000, VA 0xffffffc000230000, size     24kb, kernel RO data
PA 0x0000000080236000, VA 0xffffffc000236000, size     48kb, kernel, unmapped
PA 0x0000000080242000, VA 0xffffffc000242000, size      8kb, kernel data
PA 0x0000000080244000, VA 0xffffffc000244000, size     88kb, kernel, unmapped
PA 0x0000000080243000, VA 0xffffffc000243000, size     92kb, kernel BSS
PA 0x000000008025a000, VA 0xffffffc00025a000, size   3736kb, early RAM
PA 0x0000000080600000, VA 0xffffffc000600000, size  57344kb, usable RAM
PA 0x0000000083e00000, VA 0xffffffc003e00000, size      8kb, device tree blob
PA 0x0000000083e02000, VA 0xffffffc003e02000, size   2040kb, usable RAM
PA 0x0000000000101000, VA 0xffffffe000000000, size      4kb, memory-mapped I/O
PA 0x0000000010000000, VA 0xffffffe000002000, size      4kb, memory-mapped I/O
PA 0x0000000000100000, VA 0xffffffe000004000, size      4kb, memory-mapped I/O
PA 0x0000000010001000, VA 0xffffffe000006000, size      4kb, memory-mapped I/O
PA 0x0000000010002000, VA 0xffffffe000008000, size      4kb, memory-mapped I/O
PA 0x0000000010003000, VA 0xffffffe00000a000, size      4kb, memory-mapped I/O
PA 0x0000000010004000, VA 0xffffffe00000c000, size      4kb, memory-mapped I/O
PA 0x0000000010005000, VA 0xffffffe00000e000, size      4kb, memory-mapped I/O
PA 0x0000000010006000, VA 0xffffffe000010000, size      4kb, memory-mapped I/O
PA 0x0000000010007000, VA 0xffffffe000012000, size      4kb, memory-mapped I/O
PA 0x0000000010008000, VA 0xffffffe000014000, size      4kb, memory-mapped I/O
PA 0x000000000c000000, VA 0xffffffe000016000, size   6144kb, memory-mapped I/O
PA 0x0000000083e1d000, VA 0xffffffffffffb000, size      4kb, user kernel stack
PA 0x0000000083e0c000, VA 0xffffffffffffd000, size      4kb, user kernel stack
```

During early boot a page table is created to map the kernel from its current physical location in RAM to a known location in the upper half of the address space. Then execution is transferred there.

The kernel then creates a full mapping with all memory and found device MMIO. During the initial relocation (and then additional CPUs boot), the first kernel page (with `_entry`) needs to be mapped at its original location.

All memory is mapped starting at the split of the upper half (`0x80000000` on 32-bit and `0xffffffc000000000` on Sv39 / 64-bit). The boot loader placed the kernel at `TEXT_OFFSET` away from the RAM start, so the kernel ends up there it has been linked to.

Memory mapped IO gets virtual addresses at runtime and can end up anywhere.

---
**Overview:** [memory management](memory_management.md)

**Related:** [kernel memory map](memory_map_kernel.md) | [process memory map](memory_map_process.md) | [page](page.md)
