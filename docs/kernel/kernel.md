# VIMIX Kernel

The VIMIX kernel started as a xv6 fork and is a very basic Unix like OS for [RISC V](../riscv/RISCV.md) qemu.


## Source Code

Code organization of the kernel: [overview_directories](../overview_directories.md).


## Boot and Init

See [boot_process](overview/boot_process.md) to see how the kernel gets from the boot loader to `main()`.
See [init_overview](overview/init_overview.md) to see how main() inits all subsystems.


## Subsystems

Some [common_objects](overview/common_objects.md) are used in multiple subsystems. Some kernel objects are organized in in an [object oriented](overview/object_orientation.md) manner.

### Interrupt Handling

See [interrupts](interrupts/interrupts.md), [IPI](interrupts/IPI.md) and [timer_interrupt](interrupts/timer_interrupt.md)

### Devices / Drivers

See [devices](devices/devices.md).

### File Management

**Kernel**
- [init_filesystem](file_system/init_filesystem.md)
- [file_system](file_system/file_system.md)

**User space**
- [init_userspace](processes/init_userspace.md)

### Memory Management

For allocating kernel memory, see [memory_management](mm/memory_management.md). To learn how the virtual memory is layd out, see [memory_map_kernel](mm/memory_map_kernel.md) and [memory_map_process](mm/memory_map_process.md).

### Process Management

See [processes](processes/processes.md) and [scheduling](processes/scheduling.md).

### System Calls / User Space API

The API of the [user space](../userspace/userspace.md) to the kernel.
See [syscalls](syscalls/syscalls.md) for a list of System Calls.
To see how the application-kernel interaction works see [calling_syscall](syscalls/calling_syscall.md).


---
**Up:** [README](../../README.md)

**Boot:** [boot_process](overview/boot_process.md) | [init_overview](overview/init_overview.md)

**Subsystems:** [interrupts](interrupts.md) | [IPI](IPI.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
