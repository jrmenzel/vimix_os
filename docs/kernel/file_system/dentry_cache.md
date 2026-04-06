# Dentry Cache

A `dentry` is a path element of the [file system](file_system.md) in memory. Most importantly it stores the path element name and a pointer to the matching [inode](inode.md). It also stores a pointer to its parent and a list of children nodes. This way `dentries` build a tree representing the file layout, the `dentry cache`. Not all sub-trees are present in the `dentry cache`. But if a [file/dir](file.md) is present, the full path back to root is also present in the cache.

The main motivation for the `dentry cache` is to avoid reads from the [file system](file_system.md) (or even from disk if blocks are not cached) for all the path lookups. This is why it also contains failed file lookups / non existent files. They have their [inode](inode.md) pointer set to `NULL`. 

The in memory tree of `dentries` is using reference counting to track which entries are used. Open files, the current working directory of a [process](../processes/processes.md) and [mounts](../syscalls/mount.md) hold a reference to "their" `dentry`. [Syscalls](../syscalls/syscalls.md) temporarily hold references to `dentries`.

## Path lookup

Path to `dentry` (and by extension [[inode]]) lookup is done in `dentry_from_path()`. If a lookup in the cache fails, a lookup in the [file system](file_system.md) is performed and a new `dentry` get created and added to the cache (either holding a reference to the [inode](inode.md) or indicating a non existent file).

During path traversal, the [mode](../security/mode.md) of the [inode](inode.md) must be checked to error out if the path can not get traversed by the current [process](../processes/processes.md). This and other related metadata like [user and group IDs](../security/user_group_id.md) are considered static enough (and only get atomically changed by individual [syscalls](../syscalls/syscalls.md)) that the inodes don't get locked for reads of these fields.

This also means that `dentry->ip` and `dentry->name` stay constant after creating. If others hold a reference to the same `dentry`, [syscalls](../syscalls/syscalls.md) like [open](../syscalls/open.md) with the `O_CREAT` flag or [unlink](../syscalls/unlink.md) must insert a new `dentry` to the `dentry_cache`. This way `dentries` and [inodes](inode.md) of open files stay valid till [close](../syscalls/close.md).

## Syscalls

[Syscalls](../syscalls/syscalls.md) operating on path or file names use these path lookup functions to get references to the `dentries` to operate on. If they use [files](file.md), those will contain the `dentry`.

If a [syscall](../syscalls/syscalls.md) changes the [file system](file_system.md) layout, it is working together with the [VFS layer](vfs.md) to update the `dentry cache`. E.g. the [open syscall](../syscalls/open.md) calls via `fops_open` into the specific [file system](file_system.md) and passes in a file which already has a `dentry` attached to which the `fops_open` implementation has to attach the [inode](inode.md) of the opened file.


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [vimixfs](vimixfs/vimixfs.md) | [devfs](devfs.md) | [sysfs](sysfs.md) | [block_io](block_io.md) | [dentry_cache](dentry_cache.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
