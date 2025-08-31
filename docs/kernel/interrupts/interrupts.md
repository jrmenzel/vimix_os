# Interrupts

Without [SBI](../../riscv/SBI.md) support, there are [M-Mode](../../riscv/M-mode.md) and [S-Mode](../../riscv/S-mode.md) interrupts.
With [SBI](../../riscv/SBI.md) support, there are only [S-Mode](../../riscv/S-mode.md) interrupts.
To send an interrupt to another CPU use [IPI](IPI.md).

## Machine Mode (NON SBI ONLY)

In [M-Mode](../../riscv/M-mode.md) (call to `m_mode_trap_vector` via [SBI](../../riscv/SBI.md) ecalls), the [CLINT](../../riscv/CLINT.md) timer interrupt can get enabled.
When the timer interrupt gets triggered, it will set an [S-Mode](../../riscv/S-mode.md) software interrupt pending.


## Supervisor Mode

**Interrupts:**
- From [devices](../devices/devices.md)
- Forwarded timer interrupts from [M-Mode](../../riscv/M-mode.md) or [SBI](../../riscv/SBI.md).

At boot main will enable interrupts and set the interrupt vector `s_mode_trap_vector`.

When `s_mode_trap_vector` is called, the CPU is in [S-Mode](../../riscv/S-mode.md). It will save all registers on the stack, call `kernel_mode_interrupt_handler` and return after restoring the registers.

A Timer Interrupt can cause a `yield` and hand over the CPU to the scheduler.

### Enabling / Disabling

`cpu_enable_interrupts` / `cpu_disable_interrupts`
`cpu_push_disable_device_interrupt_stack` / `cpu_pop_disable_device_interrupt_stack`

Always disabled when a `spin_lock` is held.


## Timer Interrupts

See [timer_interrupt](timer_interrupt.md).


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](interrupts.md) | [IPI](IPI.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
