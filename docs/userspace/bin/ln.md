# ln - link

Creates a new (hard) link for a file. This means that another directory entry points to the same inode.

> ln `EXISTING_FILE` `NEW_NAME`

**Note:**
Soft links (via `ln -s ...`) are not supported.

**Returns:**
- 0 on success
**Syscall:** [link](../../kernel/syscalls/link.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [cp](cp.md) | [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mknod](mknod.md) | [rm](rm.md) | [rmdir](rmdir.md) | [stat](stat.md) | [statvfs](statvfs.md)
