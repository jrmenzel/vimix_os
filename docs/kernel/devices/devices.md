# Devices

There are two types of devices: character devices and block devices. Block devices allow random access and read/write blocks of data, e.g. hard drives. Character devices are stream devices like a UART connection or keyboard input and read/write one byte/char at a time.

The major device number defines the driver to use, the minor number can select sub devices (e.g. partitions of a disk or multiple physical UARTs handled by the same driver). In practice, minor device numbers are not used yet.


#### Access via File System

Access to a device / driver as file operations is the traditional UNIX API. To create a special file which points to a device [mknod](../syscalls/mknod.md) should be called.


#### Device Interrupts

All devices can have a `dev->irq_number` to get interrupts for that number (`dev->dev_ops.interrupt_handler`). See `handle_plic_device_interrupt()`.


### Block Devices

Block devices are normally disks with file systems. Access to block devices is routed through a block io cache: 
_Filesystem syscall_ -> _file system_ -> _block io cache_ -> _block device_
See [file_system](../file_system/file_system.md).


### Character Devices

Character devices have read / write functions for variable length of strings. 


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](devices.md) | [file_system](../file_system/file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
