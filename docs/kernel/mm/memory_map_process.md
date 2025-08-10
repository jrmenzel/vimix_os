# Memory Map of a Process

Each [user space](../../userspace/userspace.md) [process](../processes/processes.md) has its own memory map.
See [memory_map_kernel](memory_map_kernel.md) for the [kernel](kernel.md) memory map.


## User Space Processes 32-bit


Processes inherit the memory map of their parent process in [fork](../syscalls/fork.md). 

[execv](../syscalls/execv.md) will load the new binary to the locations read from the ELF file. These will be code, data and uninitialized variables in bss starting at location `USER_TEXT_START`. It's set in the Makefiles to export the same value to the kernel and the user space linker script.
Afterwards the memory map looks like this:

| VA start   | VA end    | alias            | mapped from                 | Permissions |
| ---------- | --------- | ---------------- | --------------------------- | ----------- |
| FFFF F000  | FFFF FFFF | TRAMPOLINE       | char trampoline[]           | R, X        |
| FFFF E000  | FFFF EFFF | TRAPFRAME        | proc->trapframe             | R, W        |
| FFFF 0000  | FFFF 0FFF | USER_STACK_HIGH | kalloc()                    | R, W        |
|            |           |                  |                             |             |
| heap_begin | heap_end  | heap             | set via sbrk(), grows up    | R, W, U     |
| ...        | App size  | bss              | kalloc()                    | from ELF    |
| ...        | ...       | data             | kalloc() & memcopy from ELF | from ELF    |
| 0000 1000  | text size | code             | kalloc() & memcopy from ELF | from ELF    |
| 0000 0000  | 0000 0FFF |                  | NOT MAPPED!                 |             |
- heap_begin / heap_end are members of `struct process` ([processes](../processes/processes.md)).


### Stack growing

The stack can grow dynamically: if the app tries to access an invalid page between its stack pointer and its stack, the kernel will grow the stack. This is detected in the `user_mode_interrupt_handler()`  which calls `proc_grow_stack()`.

During [scheduling](../processes/scheduling.md) before switching to the process it is checked based on the stack pointer and the current stack size if pages can be freed again. If so `proc_shrink_stack()` gets called.

**Note:** The kernel stack is still limited to one page.


### User Space 64-bit


Same as 32-bit, except that virtual memory goes up to 0x40.0000.0000.



---
**Overview:** [memory management](memory_management.md)

**Related:** [kernel memory map](memory_map_kernel.md) | [process memory map](memory_map_process.md) | [page](page.md)
