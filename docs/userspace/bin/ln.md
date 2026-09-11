# ln - link

Creates a new hard link for a file. This means that another directory entry
points to the same inode.

> ln `EXISTING_FILE` `NEW_NAME`

Create a symbolic link with:

> ln -s `TARGET` `LINK_NAME`

**Returns:**

- 0 on success

**Syscalls:** [link](../../kernel/syscalls/link.md), [symlink](../../kernel/syscalls/symlink.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [cp](cp.md) | [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mv](mv.md) | [mknod](mknod.md) | [rm](rm.md) | [rmdir](rmdir.md) | [stat](stat.md) | [statvfs](statvfs.md)
