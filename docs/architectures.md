# Supported Architectures and Arch Specifics

VIMIX supports [RISC V](riscv/RISCV.md) 32- and 64-bit.


## SBI

Booting from a [SBI](riscv/SBI.md) bootloader works (at least in `qemu`), to enable this see [build_instructions](build_instructions.md). 


## 32-bit RISC V

64-bit integer math on a 32-bit CPU requires software implementations of division etc. This is why only the 32-bit [kernel](../kernel.md) and [userspace](../../userspace/userspace.md) require `kernel/lib/div64.c`. Handling of 64-bit time values on 32-bit systems makes this necessary (see [get_time](kernel/syscalls/get_time.md)).


## 64-bit RISC V

Only 64-bit [user space](userspace/userspace.md) is supported.
To support 32-bit applications on a 64-bit [kernel](kernel/kernel.md) at least these changes would be required:
- Mark processes as 32- vs 64-bit
- Limit virtual memory addresses of 32-bit processes to 4GB.
- Switch CPU mode to execute 32-bit code before switching to process
- Switch back in interrupt
- Only store 32-bit registers at context switch (sign extend?)

Only sv39 [memory_management](kernel/mm/memory_management.md) is supported.


---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
