# User mode | U-mode

Where the [user space](../userspace/userspace.md) programs run.
The first code that runs here is from the first process which runs [init](../userspace/bin/init.md).

U-mode applications can call [S-mode](S-mode.md) [kernel](../kernel/kernel.md) functions via [syscalls](../kernel/syscalls/syscalls.md).

---
**Overview:** [RISC V](RISCV.md)
**Modes:** [M-mode](M-mode.md) [S-mode](S-mode.md) [U-mode](U-mode.md) 
