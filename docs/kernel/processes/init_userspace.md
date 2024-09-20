# Init of the userspace

One of the last init steps while the OS boots in single threaded mode (See [boot_process](overview/boot_process.md) and [init_overview](overview/init_overview.md)).


## Process 0: initcode

All processes will get created by loading an ELF file from disk and setting up the memory layout for the user process based on the metadata from the ELF file.

All processes but the first one. The problem is that the final steps of the [init_filesystem](file_system/init_filesystem.md) need to happen in a process (to call `wait()`), so there is no file system to load the ELF of the first process.

`init_userspace()` builds the first process "manually":
- `alloc_process()` creates the first process
- Virtual memory is setup (see [memory_management](mm/memory_management.md))
- The code is copied from a static array which was generated by assembling `initcode.S` and dumping the code segment into a header file (`initcode.h`).

The scheduler (see [scheduling](processes/scheduling.md)) will select this process and switch to user mode.
All the initcode does is call [execv](syscalls/execv.md) `to execute` /init.

`initcode` **is not expected to return**


## Process 1: init

`/usr/bin/init` sets up console IO for its child processes (all processes in this case) and starts a shell which then can [fork](syscalls/fork.md) and [execv](syscalls/execv.md) applications. For details see [init](../../userspace/bin/init.md).


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../boot_process.md) | [init_overview](../init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](../devices.md) | [file_system](file_system.md) | [memory_management](../memory_management.md)
[processes](../processes.md) | [scheduling](../scheduling.md) | [syscalls](../syscalls.md)