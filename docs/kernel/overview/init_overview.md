# Overview of the init process

From `main.c` on the OS init is mostly the same for all OS configs (e.g. SBI yes/no):

- parse the [device three binary](../../misc/device_tree.md)
	- gets the memory map
	- gets supported devices to pass on to init of the memory management to map known devices, also to init them later (see `struct Devices_List`)
- init console for `printk()`.
- init [memory_management](../mm/memory_management.md)
- init [processes](../processes/processes.md)
- init timer [interrupts](../interrupts/interrupts.md)
- [init_filesystem](../file_system/init_filesystem.md) part 1
- init all remaining [devices](../devices/devices.md)
- [init_userspace](../processes/init_userspace.md)
	- will create the first user space process
	- will start part 2 of [init_filesystem](../file_system/init_filesystem.md)
- all harts start in the scheduler (see [scheduling](../processes/scheduling.md))
	- See also [life_cycle_cpu](life_cycle_cpu.md).


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](boot_process.md) | [init_overview](init_overview.md)

**See also:** [life_cycle_cpu](life_cycle_cpu.md) [life_cycle_user_application](life_cycle_user_application.md)
