# RISC V

General information (see also [riscv_vs_aarch64](../misc/riscv_vs_aarch64.md)):

- Registers:
	- zero
	- 31 general purpose
- MMU:
	- 32-bit: 2-level 32-bit VA
	- 64-bit: 3-level 39-bit VA
	- 64-bit: 4-level 48-bit VA possible but unsupported by VIMIX
	- `4KB` [pages](../kernel/mm/page.md).
- Common firmware: Supervisor Binary Interface [SBI](SBI.md)
- Execution modes from high privileged to low privileged:
	- [M-mode](M-mode.md) (for [SBI](SBI.md) or timer [interrupts](../kernel/interrupts/interrupts.md))
    - [S-mode](S-mode.md) (for the [kernel](../kernel/kernel.md))
    - [U-mode](U-mode.md) (for the [user space](../userspace/userspace.md))
- Interrupt Controllers:
	- [PLIC](PLIC.md)
	- [CLINT](CLINT.md)

## Assembly

GCC inline assembly syntax:
https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
https://gcc.gnu.org/onlinedocs/gcc/Local-Register-Variables.html

## Documentation

Various specifications: https://wiki.riscv.org/display/HOME/RISC-V+Technical+Specifications

---
**Overview:** [architectures](../misc/architectures.md) | [RISC V vs ARM 64](../misc/riscv_vs_aarch64.md)

**RISC V**: [CLINT](CLINT.md) | [ecall](ecall.md) | [M-mode](M-mode.md) | [PLIC](PLIC.md) | [RISCV](RISCV.md) | [S-mode](S-mode.md) | [SBI](SBI.md) | [U-mode](U-mode.md)
