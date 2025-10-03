# SysFS

SysFS is modeled at a high level after the Linux sys fs. It gets [mounted](../../syscalls/mount.md) at `/sys` by the first [process](../../processes/processes.md) [init](../../../userspace/bin/init.md). This pseudo file system exposes `kobject`s and there attributes. 

## kobject

All `kobject`s are organized in a tree starting at `g_kobjects_root`. Each `kobject` holds a pointer to its parent and a list of child nodes.

SysFS represents each `kobject` as a [directory](../directory.md) with the children being the sub directories and the attributes being files (all children are in a `kset` which is a `kobject` itself on Linux, also the hierarchy and files differ). Attributes and related callbacks are contained in `struct kobj_type`.

### Registration and SysFS Nodes

Each `kobject` registers itself with SysFS via `sysfs_register_kobject()` at which time SysFS generates `sysfs_node`s representing the in memory [file system](../file_system.md). [Inodes](../inode.md) are generated on demand. The `kobject` itself also holds an array of `sysfs_node` pointers, one for the dir and one per file / attribute. This way the same `kobj_type` object can be used for similar objects (e.g. [processes](../../processes/processes.md)).

### Attributes

Each `sysfs_attribute` hold the unique data for that file (name, access rights). When [user space](../../../userspace/userspace.md) reads/writes one of these files, `sysfs_iops_read()` / `sysfs_fops_write()` will call the `kobject` specific read/write callbacks from `sysfs_ops` (in `kobj_type`). These object type specific functions get the attribute index to distinguish between the different files. 


### Reading from user space

Every time the user space calls a read/write function on a SysFS file, the content gets regenerated. To get the expected results, a single [read](../../syscalls/read.md) or [write](../../syscalls/write.md) [syscall](../../syscalls/syscalls.md) should be made. E.g. read a SysFS file with one `read(fd, buffer, n)` call instead of something like `fgets()` that will call `read()` multiple times.


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [vimixfs](../vimixfs/vimixfs.md) | [devfs](devfs.md) | [sysfs](sysfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
