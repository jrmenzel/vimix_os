# Init of the File System


## Part 1: On the Kernel Boot CPU

The boot CPU inits in `main.c`:
- init buffer cache
- init [inode](inode.md) table
- init [file](file.md) table
It also picks the `/` device (`ROOT_DEVICE_NUMBER`).


## Part 2: In a Kernel Process

The actual file system gets initialized from the first spawned process in `forkret()` (calling `mount_root(ROOT_DEVICE_NUMBER, "xv6fs");`). This is triggered via [init_userspace](../processes/init_userspace.md) and [mounts](../syscalls/mount.md) the root file system.

While belonging to the first process (`init`), this still runs in [S-mode](../../riscv/S-mode.md).


---
**Overview:** [kernel](../kernel.md) | [file_system](file_system.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [xv6fs](xv6fs/xv6fs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
