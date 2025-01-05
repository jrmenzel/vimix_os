# File System

A UNIX file system stores [inodes](inode.md), which represent [files](file.md), [directories](directory.md) or [devices](../devices/devices.md). File systems are added to the system by [mounting](../syscalls/mount.md) them into existing [directories](directory.md) (except for the root file system).


**Software stack:**
- File System related [system calls](../syscalls/syscalls.md) 
- [virtual file system](vfs.md)
- [xv6 File System](xv6fs/xv6fs.md)
	- [xv6fs Log](xv6fs/xv6fs_log.md)
- [Block IO Cache](block_io.md)
- [device drivers](../devices/devices.md) read()/write()


## Supported File Systems

File systems supported:
- mostly compatible [xv6 filesystem](xv6fs/xv6fs.md) (`kernel/fs/xv6fs`)
- [devfs](devfs/devfs.md) for `/dev`


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [xv6fs](xv6fs/xv6fs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
