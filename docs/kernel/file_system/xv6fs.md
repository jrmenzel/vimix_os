# xv6fs

Modified variant of the [file_system](file_system.md) found in xv6, which is designed after the original file system of UNIX version 6.


## Disk Layout

The [inodes](inode.md) are stored sequentially on disk in an array. The inode number is the index into this array.


## Changes compared to xv6

- refactored and renamed some defines


## Main Limitations

- No support for UID, GID, access rights...
- [file](file.md) name limit is 14 chars.
- low file size limit


---
**Overview:** [kernel](../kernel.md)
**Boot:**
[boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)
**Subsystems:**
[interrupts](../interrupts/interrupts.md) | [devices](../devices/devices.md) | [file_system](file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
