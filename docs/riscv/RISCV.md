# RISC V


Platform:
- VIMIX [boot](boot.md) requirements.
- Supervisor Binary Interface [SBI](SBI.md)
- Execution modes from high privileged to low privileged: 
	- [M-mode](M-mode.md) (for the [user space](../userspace/userspace.md))
	- [S-mode](S-mode.md) (for the [kernel](../kernel/kernel.md))
	- [U-mode](U-mode.md) (for [SBI](SBI.md) or timer [interrupts](../kernel/interrupts/interrupts.md))
- [PLIC](PLIC.md)
- [CLINT](CLINT.md)


## Assembly

GCC inline assembly syntax:
https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
https://gcc.gnu.org/onlinedocs/gcc/Local-Register-Variables.html


## Documentation

Various specifications: https://wiki.riscv.org/display/HOME/RISC-V+Technical+Specifications


---
**Overview:** [architectures](../architectures.md)
