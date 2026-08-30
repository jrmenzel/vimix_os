# Machine mode | M-mode

Highest privilege mode.

**If VIMIX runs bare-metal:**

- The [kernel](../kernel/kernel.md) boots in M-mode (see [boot process](../kernel/overview/boot_process.md)) but quickly switches to [S-mode](S-mode.md).
- A M-mode interrupt handler will answer to [SBI](SBI.md) ecalls and setup the [PLIC](PLIC.md) timer. (see `m_mode_trap_vector.S`).

**If VIMIX runs on OpenSBI:**

- All source files starting with `m_mode*` will be ignored.
- The [kernel](../kernel/kernel.md) boots in [S-mode](S-mode.md).
- Only [OpenSBI](SBI.md) runs in M-mode and provides e.g. timer interrupts to [S-mode](S-mode.md).

---
**Overview:** [architectures](../misc/architectures.md) | [RISC V vs ARM 64](../misc/riscv_vs_aarch64.md)

**RISC V**: [CLINT](CLINT.md) | [ecall](ecall.md) | [M-mode](M-mode.md) | [PLIC](PLIC.md) | [RISCV](RISCV.md) | [S-mode](S-mode.md) | [SBI](SBI.md) | [U-mode](U-mode.md)

**Modes:** [M-mode](M-mode.md) [S-mode](S-mode.md) [U-mode](U-mode.md)
