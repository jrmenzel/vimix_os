# Memory Map of a Process

Each [user space](../../userspace/userspace.md) [process](../processes/processes.md) has its own memory map.
See [memory_map_kernel](memory_map_kernel.md) for the [kernel](kernel.md) memory map.


## User Space Processes 32-bit


Processes inherit the memory map of their parent process in [fork](../syscalls/fork.md). 

[execv](../syscalls/execv.md) will load the new binary to the locations read from the ELF file. These will be code, data and uninitialized variables in bss starting at location `USER_TEXT_START`. It's set in the Makefiles to export the same value to the kernel and the user space linker script.

Afterwards the memory map looks like this:
```
PA 0x80075000, VA 0x00400000, size     12kb, user text
PA 0x83e30000, VA 0x00403000, size     20kb, user text
PA 0x83e36000, VA 0x00408000, size      4kb, user data
PA 0x83e37000, VA 0x7ffef000, size      4kb, user stack
PA 0x83e0d000, VA 0x7fffe000, size      4kb, user trapframe
PA 0x8002f000, VA 0x7ffff000, size      4kb, user trampoline
```

- heap_begin / heap_end are members of `struct process` ([processes](../processes/processes.md)).


### Stack growing

The stack can grow dynamically: if the app tries to access an invalid page between its stack pointer and its stack, the kernel will grow the stack. This is detected in the `user_mode_interrupt_handler()`  which calls `proc_grow_stack()`.

During [scheduling](../processes/scheduling.md) before switching to the process it is checked based on the stack pointer and the current stack size if pages can be freed again. If so `proc_shrink_stack()` gets called.

**Note:** The kernel stack is still limited to one page.


### User Space 64-bit


Same as 32-bit, except that virtual memory goes up to 0x40.0000.0000.

Below is a typical user space memory map on 64-bit:
```
Memory Map:
PA 0x0000000083e3a000, VA 0x0000000000400000, size     24kb, user text
PA 0x00000000802a0000, VA 0x0000000000406000, size      8kb, user text
PA 0x00000000802a4000, VA 0x0000000000408000, size      4kb, user data
PA 0x00000000802a5000, VA 0x0000003ffffef000, size      4kb, user stack
PA 0x0000000083e1c000, VA 0x0000003fffffe000, size      4kb, user trapframe
PA 0x000000008022f000, VA 0x0000003ffffff000, size      4kb, user trampoline
```


---
**Overview:** [memory management](memory_management.md)

**Related:** [kernel memory map](memory_map_kernel.md) | [process memory map](memory_map_process.md) | [page](page.md)
