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
**File Management:** [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [rm](rm.md) | [stat](stat.md)
