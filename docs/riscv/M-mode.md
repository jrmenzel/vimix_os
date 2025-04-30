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
**Overview:** [RISC V](RISCV.md)

**Modes:** [M-mode](M-mode.md) [S-mode](S-mode.md) [U-mode](U-mode.md) 
