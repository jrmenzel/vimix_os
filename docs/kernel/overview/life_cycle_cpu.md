# The life cycle of a CPU

**Boot:** (for details, see [boot_process](boot_process.md))
`entry.S` -> `start.c` -> `main.c` calls `schedule()` 

**schedule():**
`schedule()` enters an infinite loop and will never return.
From now on the harts will do this forever:

- look for a process to run (for details see [scheduling](../processes/scheduling.md))
	- If there is none, wait for an interrupt (`wfi` assembly instruction, CPU sleeps). 
		- A timer interrupt or a device interrupt will wake this up (see [interrupts](../interrupts/interrupts.md)).
			- This calls `kernel_mode_interrupt_handler()`
			- Both interrupt types will quickly do their things and then return to the scheduler which starts its next loop.
				- (There is no process the timer interrupt could force to yield)
	- If there is a process, `context_switch()` will load the processes context/registers, and return to that context.
		- A new process starts in `forkret()` (set in `alloc_process()`) which will return to user mode and thus start the application.
		- An existing process returns to its `yield()` call in [S-mode](../../riscv/S-mode.md).
			- This process can switch between kernel and user space until a `wait()` or timer interrupt ([interrupts](../interrupts/interrupts.md)) will have the process call `yield()`. This switches back to the schedulers context via `context_switch()`.


---
**Overview:** [kernel](../kernel.md)
**Boot:**
[boot_process](boot_process.md) | [init_overview](init_overview.md)
**See also:**
[life_cycle_cpu](life_cycle_cpu.md) [life_cycle_user_application](life_cycle_user_application.md)
