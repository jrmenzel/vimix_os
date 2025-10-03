# Virtual File System

The virtual file system is an abstraction which allows the kernel to work with [inodes](inode.md) and [files](file.md) independent on the [file system](file_system.md) they belong to. It's basically a set of functions each file system needs to implement. Each inode then holds pointers to these functions which the kernel functions call. As inodes are created by file system specific functions, they are created with the matching set of function pointers.

The functions each file system needs to implement are defined in `vfs_operations.h`. They are defined in three structs: one for functions related to the super block, one for inode related operations and one for file related ones.

At init time each file system calls `register_file_system()` with a struct containing everything the kernel needs to mount a file system of that type. 


### Mounting

When a file system should be mounted the kernel looks through all registered file systems, matches the name and calls the matching `init_fs_super_block()` function. This sets the file system related functions and also adds "private" (fs specific) data to the `super_block`.


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [vimixfs](vimixfs/vimixfs.md) | [devfs](devfs.md) | [sysfs](sysfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
