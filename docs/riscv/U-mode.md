# User mode | U-mode

Where the [user space](../userspace/userspace.md) programs run.
The first code that runs here is from the first process which runs [init](../userspace/bin/init.md).

U-mode applications can call [S-mode](S-mode.md) [kernel](../kernel/kernel.md) functions via [syscalls](../kernel/syscalls/syscalls.md).

---
**Overview:** [architectures](../misc/architectures.md) | [RISC V vs ARM 64](../misc/riscv_vs_aarch64.md)

**RISC V**: [CLINT](CLINT.md) | [ecall](ecall.md) | [M-mode](M-mode.md) | [PLIC](PLIC.md) | [RISCV](RISCV.md) | [S-mode](S-mode.md) | [SBI](SBI.md) | [U-mode](U-mode.md)

**Modes:** [M-mode](M-mode.md) [S-mode](S-mode.md) [U-mode](U-mode.md)
