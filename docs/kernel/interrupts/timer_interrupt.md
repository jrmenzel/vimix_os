# Timer Interrupt


Timer [interrupts](interrupts.md) are created by [PLIC](../../riscv/PLIC.md) in [M-mode](../../riscv/M-mode.md). If the platform supports [SBI](../../riscv/SBI.md), it will create timer interrupts in [S-mode](../../riscv/S-mode.md). 

The [kernel](kernel.md) handles timer interrupts in `trap.c`.


## Interval

Timer interrupts happen at a fixed interval set via `TIMER_INTERRUPTS_PER_SECOND`. This also dictates the timer granularity for [ms_sleep](syscalls/ms_sleep.md): E.g. at `100` for `TIMER_INTERRUPTS_PER_SECOND`, each timer interrupt happens after 10 milliseconds.
[ms_sleep](syscalls/ms_sleep.md) will at least take one interrupt interval.


## Timer Interrupt while a process executes

If an application was running when the timer [interrupts](interrupts.md) occurred, it will:
- Call `u_mode_trap_vector` (like a [syscall](syscalls/syscalls.md))
- `user_mode_interrupt_handler()` will call `yield()` for the process
- `yield()` basically calls `schedule()` and switch back to the scheduler
- Once rescheduled it basically returns to where it was interrupted in user mode

A timer interrupt can also happen while the process in in kernel mode (e.g. during a [syscall](syscalls/syscalls.md)). It also yields but on return will continue with the process in kernel mode.


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](interrupts.md) | [IPI](IPI.md) | [devices](../devices/devices.md) | [file_system](../file_system/file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
