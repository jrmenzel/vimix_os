# Directory

A directory is a list of [inodes](inode.md) and [file](file.md) names. When a directory is created by creating its [inode](inode.md) (in `inode_open_or_create()`), the [kernel](../kernel.md) will create two entries: `.` with the same inode as the new dir and `..` with the inode of the parent dir.

## Related Syscalls and Apps

Related [syscalls](../syscalls/syscalls.md):
- Create dir: [mkdir](../syscalls/mkdir.md) (app [mkdir](../../userspace/bin/mkdir.md))
- Get statistics: [fstat](../syscalls/fstat.md) (app [stat](../../userspace/bin/stat.md))
- Get directory entries: [get_dirent](../syscalls/get_dirent.md) (app [ls](../../userspace/bin/ls.md))
- Delete dir: [rmdir](../syscalls/rmdir.md) (app [rmdir](../../userspace/bin/rmdir.md))

Loosely related:
- Change the [processes](../processes/processes.md)' CWD: [chdir](../syscalls/chdir.md) (app: `cd` in [sh](../../userspace/bin/sh.md))


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [xv6fs](xv6fs/xv6fs.md) | [devfs](devfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
