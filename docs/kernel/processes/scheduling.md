# Scheduling

## General

`schedule()` runs on all cores in an infinite loop after the boot code (see [life_cycle_cpu](../overview/life_cycle_cpu.md)).
When they find a run-able process, the switch to it (`context_switch()`).
All processes have a [kernel](kernel.md) context which was active when they called `yield()` (e.g. from a timer [interrupts](../interrupts/interrupts.md)) or from when they were created.

From the processes perspective: [life_cycle_user_application](../overview/life_cycle_user_application.md).

**Note:**
- Tasks can switch CPUs. 
- Tasks can switch CPUs while in Supervisor Mode

## Scheduling Algorithm

- A primitive **round-robin** scheduler without any priorities.
- All CPUs go over the same global process list, CPUs can be switched at random.


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../boot_process.md) | [init_overview](../init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](../devices.md) | [file_system](file_system.md) | [memory_management](../memory_management.md)
[processes](../processes.md) | [scheduling](../scheduling.md) | [syscalls](../syscalls.md)
