# Inode

An inode is an unnamed object in the [file_system](file_system.md) tree, it represents a [file](file.md), a directory or a [device](../devices/devices.md).
Open inodes (e.g. of open files) are stored in an inode array in the [kernel](../kernel.md). 

The `struct inode` kernel structure holds 
- metadata about the inode (e.g. size, UID, GID)
- members for inode management (e.g. a lock, reference counter)
- in case of a [file](file.md) or directory:
	- reference to the device memory where the data is stored

UNIX [file systems](file_system.md) store the inodes to disk.


## Inodes in the Kernel

The inodes go through the following life cycle before they can be used by other parts of the [kernel](../kernel.md).

Inodes get allocated in `inode_alloc()`. De-allocation happens if the reference count falls to `0` in a subsequent `inode_put()` call.

Init of a inode happens in `inode_open_or_create()` which might call `inode_alloc()`.

A typical sequence is:
```C
ip = iget(dev, inum);
inode_lock(ip);
// ... examine and modify ip->xxx ...
inode_unlock(ip);
inode_put(ip);
```

`inode_lock()` is separate from `iget()` so that [syscalls](../syscalls/syscalls.md) can get a long-term reference to an inode (as for an open file) and only lock it for short periods (e.g., in read()).

The separation also helps avoid deadlock and races during path name lookup. `iget()` increments `ip->ref` so that the inode stays in the table and pointers to it remain valid.



## Inode Number

An inode number is a unique [file](file.md) identifier per [file system](file_system.md). In the kernel the combination of inode number and device ID of the file systems device is unique.


## User space

The application [stat](../../userspace/bin/stat.md) can print the inodes of files using the [syscall](../syscalls/syscalls.md) [fstat](../syscalls/fstat.md).


---
**Overview:** [kernel](../kernel.md)
**Boot:**
[boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)
**Subsystems:**
[interrupts](../interrupts/interrupts.md) | [devices](../devices/devices.md) | [file_system](file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
