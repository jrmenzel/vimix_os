# ARM 64 / aarch64

The `64-bit` ARM architecture is officially called `aarch64` but this will be used interchangeably with `ARM 64` in this project.

General information (see also [riscv_vs_aarch64](../misc/riscv_vs_aarch64.md)):

- Registers:
	- zero
	- Stack pointer `sp`
	- 30 general purpose
- MMU:
	- 64-bit: 4-level 48-bit VA
	- `4KB` [pages](../kernel/mm/page.md), `16KB` and `64KB` possible but unsupported by VIMIX
- Common firmware: PSCI
- Execution modes from high privileged to low privileged:
	- EL3
	- EL2
	- EL1  (for the [kernel](../kernel/kernel.md))
	- EL0  (for the [user space](../userspace/userspace.md))
- Interrupt Controllers:
	- GIC v2

---
**Overview:** [architectures](../misc/architectures.md) | [RISC V vs ARM 64](../misc/riscv_vs_aarch64.md)
