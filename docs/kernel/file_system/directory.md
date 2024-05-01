# Directory

A directory is a list of [inodes](inode.md) and [file](file.md) names. When a directory is created by creating its [inode](inode.md) (in `inode_open_or_create()`), the [kernel](../kernel.md) will create two entries: `.` with the same inode as the new dir and `..` with the inode of the parent dir.

## Related Syscalls and Apps

Related [syscalls](../syscalls/syscalls.md):
- Create dir: [mkdir](../syscalls/mkdir.md) (app [mkdir](../../userspace/bin/mkdir.md))
- Get statistics: [fstat](../syscalls/fstat.md) (app [stat](../../userspace/bin/stat.md))
- Get directory entries: [get_dirent](../syscalls/get_dirent.md) (app [ls](../../userspace/bin/ls.md))
- Delete dir: [unlink](../syscalls/unlink.md) 

Loosely related:
- Change the [processes](../processes/processes.md)' CWD: [chdir](../syscalls/chdir.md) (app: `cd` in [sh](../../userspace/bin/sh.md))


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](../devices/devices.md) | [file_system](file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
