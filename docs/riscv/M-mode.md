# Machine mode | M-mode

Highest privilege mode.

**If VIMIX runs bare-metal:**
- The [kernel](../kernel/kernel.md) boots in M-mode (see [boot_process](../kernel/overview/boot_process.md)) but quickly switches to [S-mode](S-mode.md).
- The timer [interrupts](../kernel/interrupts/interrupts.md) runs in M-mode (see `m_mode_trap_vector.S`).

**If VIMIX runs on OpenSBI:**
- Only [OpenSBI](SBI.md) runs in M-mode and provides e.g. timer interrupts to [S-mode](S-mode.md).


---
**Overview:** [RISC V](RISCV.md)
**Modes:** [M-mode](M-mode.md) [S-mode](S-mode.md) [U-mode](U-mode.md) 
