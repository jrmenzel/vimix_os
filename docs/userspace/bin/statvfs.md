# statvfs - print file system statistics

Prints file system stats like total and used block count. Basically calling [statvfs](../../kernel/syscalls/statvfs.md) syscall and printing the result.

> statvfs `FILE`


**Returns:**
- 0 on success
**Syscall:** [statvfs](../../kernel/syscalls/statvfs.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [cp](cp.md) | [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mknod](mknod.md) | [rm](rm.md) | [rmdir](rmdir.md) | [stat](stat.md) | [statvfs](statvfs.md)
