# Init of the userspace

One of the last init steps while the OS boots in single threaded mode (See [boot_process](overview/boot_process.md) and [init_overview](overview/init_overview.md)).


## Process 1: init

All processes will get created by loading an ELF file from disk and setting up the memory layout for the user process based on the metadata from the ELF file.

Loading the first binary (for process [init](../../userspace/bin/init.md)) has to happen after the [file_system](../file_system/file_system.md) got initialized. The problem is that the final steps of the [init_filesystem](file_system/init_filesystem.md) need to happen in a process (to be able to call `wait()`). So the first process is setup in `userspace_init()` without loading the binary. The [scheduler](scheduling.md) will call `forkret()` which will during its first call [mount](../syscalls/mount.md) `/` and then load the [init binary](../../userspace/bin/init.md).

`/usr/bin/init` sets up console IO for its child processes (all processes in this case) and starts a shell which then can [fork](syscalls/fork.md) and [execv](syscalls/execv.md) applications. For details see [init](../../userspace/bin/init.md).


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../boot_process.md) | [init_overview](../init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](../devices.md) | [file_system](file_system.md) | [memory_management](../memory_management.md)
[processes](../processes.md) | [scheduling](../scheduling.md) | [syscalls](../syscalls.md)
