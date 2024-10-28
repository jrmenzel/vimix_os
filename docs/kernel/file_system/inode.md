# Inode

An inode is an unnamed object in the [file_system](file_system.md) tree, it represents a [file](file.md), a [directory](directory.md) or a [device](../devices/devices.md).
Open inodes (e.g. of open files) are managed by the owning [file systems](file_system.md).

The `struct inode` kernel structure holds 
- metadata about the inode (e.g. size, UID, GID)
- members for inode management (e.g. a lock, reference counter)
- in case of a [file](file.md) or directory:
	- reference to the device memory where the data is stored

UNIX [file systems](file_system.md) store the inodes to disk.


## Inodes in the Kernel

The inodes go through the following life cycle before they can be used by other parts of the [kernel](../kernel.md).

Inodes get allocated and initialized in a [file system](file_system.md) specific `alloc()` function. This way the file system can add its own data to an inode (by deriving from the base inode struct, see [object_orientation](../overview/object_orientation.md)). De-allocation happens if the reference count falls to `0` in a subsequent `inode_put()` call.


A typical sequence is:
```C
ip = inode_from_path(path);
inode_lock(ip);
// ... examine and modify ip->xxx ...
inode_unlock(ip);
inode_put(ip);
```

`inode_lock()` is separate from "get functions" so that [syscalls](../syscalls/syscalls.md) can get a long-term reference to an inode (e.g. for an open file) and only lock it for short periods (e.g., in read()). The separation also helps avoid deadlock and races during path name lookup. 

As inodes store a pointer to the [file system](file_system.md) they belong to, the inodes them selves can be managed by the file system driver globally for all mounted instances. E.g. [xv6fs](xv6fs.md) has one pool of inodes for all mounted file systems. 


## Inode Number

An inode number is a unique [file](file.md) identifier per [file system](file_system.md). In the kernel the combination of inode number and device number of the file systems device is unique. The inode number alone is not.


## User space

The application [stat](../../userspace/bin/stat.md) can print the inodes of files using the [syscall](../syscalls/syscalls.md) [fstat](../syscalls/fstat.md).


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [xv6fs](xv6fs.md) | [xv6fs_log](xv6fs_log.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
