# File System

A UNIX file system stores [inodes](inode.md), which represent [files](file.md), [directories](directory.md) or [devices](../devices/devices.md). File systems are added to the system by [mounting](../syscalls/mount.md) them into existing [directories](directory.md) (except for the root file system).


**Software stack:**

The software stack coming from a [user space](../../userspace/userspace.md) application looks the same until the [virtual file system](vfs.md) calls into different file systems. Runtime based, "virtual" file systems like [sysfs](sysfs/sysfs.md) are self contained while "real" file systems backed by a disk call into [Block IO](block_io.md) and finally a [block device](../devices/devices.md).

| "real" FS                                              | common                                                      | "virtual" FS                                      |
| ------------------------------------------------------ | ----------------------------------------------------------- | ------------------------------------------------- |
|                                                        | File System related [system calls](../syscalls/syscalls.md) |                                                   |
|                                                        | [virtual file system](vfs.md)                               |                                                   |
| [Vimix File System](vimixfs/vimixfs.md)                      |                                                             | [devfs](devfs/devfs.md) / [sysfs](sysfs/sysfs.md) |
| [VimixFS Log](vimixfs/vimixfs_log.md)                        |                                                             |                                                   |
| [Block IO Cache](block_io.md)                          |                                                             |                                                   |
| [device drivers](../devices/devices.md) read()/write() |                                                             |                                                   |


## Supported File Systems

File systems supported:
- mostly compatible [xv6 file system](vimixfs/vimixfs.md) (`kernel/fs/vimixfs`)
- [devfs](devfs/devfs.md) for `/dev`
- [sysfs](sysfs/sysfs.md) for `/sys`


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [vimixfs](vimixfs/vimixfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
