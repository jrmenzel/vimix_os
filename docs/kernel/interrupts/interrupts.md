# Interrupts

Without [SBI](../../riscv/SBI.md) support, there are [M-mode](../../riscv/M-mode.md) and [S-mode](../../riscv/S-mode.md) interrupts.
With [SBI](../../riscv/SBI.md) support, there are only [S-mode](../../riscv/S-mode.md) interrupts.


## Machine Mode (NON SBI ONLY)

**Interrupts:**
-  Timer via [PLIC](../../riscv/PLIC.md)

At boot while still in [M-mode](../../riscv/M-mode.md), the [CLINT](../../riscv/CLINT.md) timer interrupt gets enabled. It will trigger `m_mode_trap_vector`. 

This assembly routine will save a few registers, reprogram the timer for the next interrupt and trigger a [S-mode](../../riscv/S-mode.md) software interrupt to handle the timer.

Then it returns. 

### Enabling / Disabling

[M-mode](../../riscv/M-mode.md) / Timer Interrupts stay enabled once they are enabled (but Timer interrupts are handled in [S-mode](../../riscv/S-mode.md), where they can be suppressed).


## Supervisor Mode

**Interrupts:**
- From [devices](../devices/devices.md)
- Forwarded timer interrupts from [M-mode](../../riscv/M-mode.md) or [SBI](../../riscv/SBI.md).

At boot main will enable interrupts and set the interrupt vector `s_mode_trap_vector`.

When `s_mode_trap_vector` is called, the CPU is in [S-mode](../../riscv/S-mode.md). It will save all registers on the stack, call `kernel_mode_interrupt_handler` and return after restoring the registers.

A Timer Interrupt can cause a `yield` and hand over the CPU to the scheduler.

### Enabling / Disabling

`cpu_enable_device_interrupts` / `cpu_disable_device_interrupts`
`cpu_push_disable_device_interrupt_stack` / `cpu_pop_disable_device_interrupt_stack`

Always disabled when a `spin_lock` is held.


## Timer Interrupts

See [timer_interrupt](timer_interrupt.md).


---
**Overview:** [kernel](../kernel.md)
**Boot:**
[boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)
**Subsystems:**
[interrupts](interrupts.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
