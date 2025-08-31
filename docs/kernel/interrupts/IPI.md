# Inter Processor Interrupt

Allows one CPU to send a software interrupt to another CPU.

The sending CPU calls `ipi_send_interrupt()` with a type and optional data. The receiving CPU handles the interrupt in `handle_ipi_interrupt()`.

IPIs are used to stop the scheduling on all CPUs during a `panic()` or  [shutdown / reboot](../syscalls/reboot.md).

On [RISCV](../../riscv/RISCV.md) IPIs are implemented via [SBI calls](../../riscv/SBI.md) which configure [CLINT](../../riscv/CLINT.md) to issue an [M-mode](../../riscv/M-mode.md) interrupt on the other CPU which then schedules an interrupt in [S-mode](../../riscv/S-mode.md).


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](interrupts.md) | [IPI](IPI.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
