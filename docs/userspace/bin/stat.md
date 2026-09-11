# stat

Prints file or directory stats like size and [inode](../../kernel/file_system/inode.md) number. Symbolic links are displayed as links rather than followed.

> stat `FILE`

**Returns:**

- 0 on success

**Syscall:** [lstat](../../kernel/syscalls/lstat.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [cp](cp.md) | [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mv](mv.md) | [mknod](mknod.md) | [rm](rm.md) | [rmdir](rmdir.md) | [stat](stat.md) | [statvfs](statvfs.md)
