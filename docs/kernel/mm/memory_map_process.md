# Memory Map of a Process

Each [user space](../../userspace/userspace.md) [process](../processes/processes.md) has its own memory map.
See [memory_map_kernel](memory_map_kernel.md) for the [kernel](kernel.md) memory map.


## User Space Processes 32-bit


Processes inherit the memory map of their parent process in [fork](../syscalls/fork.md). 
See memory map of the init code / first process (see [init_userspace](../processes/init_userspace.md)) below.
Note that all the init code does is call [execv](../syscalls/execv.md) for the [init](../../userspace/bin/init.md) ELF file. So it does not have a stack!

| VA start  | VA end    | alias      | mapped from                   | Permissions |
| --------- | --------- | ---------- | ----------------------------- | ----------- |
| FFFF F000 | FFFF FFFF | TRAMPOLINE | char trampoline[]             | R, X        |
| FFFF D000 | FFFF DFFF | TRAPFRAME  | proc->trapframe               | R, W        |
| 0000 0000 | 0000 1000 | init code  | kalloc() & g_initcode memcopy | R, W, X, U  |

[execv](../syscalls/execv.md) will load the new binary to the locations read from the ELF file. These will be code, data and uninitialized variables in bss starting at location 0x0000.0000. See `user.ld`.
Afterwards the memory map looks like this:

| VA start  | VA end    | alias      | mapped from                 | Permissions |
| --------- | --------- | ---------- | --------------------------- | ----------- |
| FFFF F000 | FFFF FFFF | TRAMPOLINE | char trampoline[]           | R, X        |
| FFFF D000 | FFFF DFFF | TRAPFRAME  | proc->trapframe             | R, W        |
|           |           |            |                             |             |
|           |           | heap       | set via sbrk(), grows up    | R, W, U     |
| AS + 2P   |           | stack      | kalloc()                    | R, W, U     |
| AS + 1P   |           | (guard)    |                             | W           |
| ...       | App size  | bss        | kalloc()                    | from ELF    |
| ...       | ...       | data       | kalloc() & memcopy from ELF | from ELF    |
| 0000 0000 | text size | code       | kalloc() & memcopy from ELF | from ELF    |
- `AS + 1P`: App size (including bss) + 1 page (at next page boundry)


### User Space 64-bit


Same as 32-bit, except that virtual memory goes up to 0x40.0000.0000.



---
**Overview:** [kernel](../kernel.md)
**Boot:**
[boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)
**Subsystems:**
[interrupts](interrupts.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
