# Init of the File System


## Part 1: On the Kernel Boot CPU

The boot CPU inits in `main.c`:
- init buffer cache
- init [virtual file system](vfs.md)
	- this inits all individual [file systems](file_system.md), which in turn inits the [inode](inode.md) table(s)
- init [file](file.md) table
- pick the `/` device (`ROOT_DEVICE_NUMBER`) - to be mounted in part 2


## Part 2: In a Kernel Process

The actual file system gets initialized from the first spawned process in `forkret()` (calling `mount_root(ROOT_DEVICE_NUMBER, "xv6fs");`). This is triggered via [init_userspace](../processes/init_userspace.md) and [mounts](../syscalls/mount.md) the root file system.

While belonging to the first process (`init`), this still runs in [S-mode](../../riscv/S-mode.md).

The reason the init is placed here is that mounting the file system requires reads from disk (at least the file system super block to check some meta data). Reads will try to set the current [processes](../processes/processes.md) sleeping until the data is ready. This would not work until a process is set up.


---
**Overview:** [kernel](../kernel.md) | [file_system](file_system.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [xv6fs](xv6fs/xv6fs.md) | [devfs](devfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
