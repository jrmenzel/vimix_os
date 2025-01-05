# File


## In C

Defined in `stdio.h` (see also [stdio](../../misc/stdio.md)), a struct around a **file descriptor**, which is a signed integer.


## In the Kernel

The **file descriptor** is an index into the array of open files of a process (`process->files`, limit: `MAX_FILES_PER_PROCESS`). Each open file is represented by a `struct file` struct. 

A file can be:
- A regular file in a [file_system](file_system.md)
	- In this case the file points to an [inode](inode.md)
- A directory
	- In this case the file also points to an [inode](inode.md)
- A [device](../devices/devices.md)
	- Stores the `dev_t` (but could also use the `dev_t` from the inode...)
- A [pipe](../syscalls/pipe.md)


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [xv6fs](xv6fs/xv6fs.md) | [devfs](devfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
