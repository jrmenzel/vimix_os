# ecall (Environment Call) instruction

> The ECALL instruction is used to make a request to the supporting execution environment, which is usually an operating system. The ABI for the system will define how parameters for the environment request are passed, but usually these will be in defined locations in the integer register file.
- RISC V Spec 2.1 (https://riscv.org/wp-content/uploads/2016/06/riscv-spec-v2.1.pdf)

[U-mode](U-mode.md) call to [S-mode](S-mode.md) or [S-mode](S-mode.md) call to [M-mode](M-mode.md).
Used for [syscalls](../kernel/syscalls/syscalls.md).


---
**Overview:** [RISC V](RISCV.md)
