# Processes


The main struct to hold all data of one process is `struct process` (in Linux it's `struct task_struct`, in xv6 it's `struct proc`).

## Overview

- The first user space process is created in `init_userspace()` ([init_userspace](init_userspace.md)).
- Processes go through the [life_cycle_user_application](../overview/life_cycle_user_application.md).
	- In short: New processes are created via `alloc_process()`, triggered by sys call [fork](syscalls/fork.md).
	- They exit by calling [exit](syscalls/exit.md) or if they are terminated explicitly via [kill](kill).
- See also [scheduling](scheduling.md).


## User Mode

There are a number of [syscalls](syscalls/syscalls.md) to modify process state.

---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../boot_process.md) | [init_overview](../init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](../devices.md) | [file_system](file_system.md) | [memory_management](../memory_management.md)
[processes](../processes.md) | [scheduling](../scheduling.md) | [syscalls](../syscalls.md)
