# rm - remove

Deletes [files](../../kernel/file_system/file.md), but not [directories](../../kernel/file_system/directory.md) (use [rmdir](rmdir.md)).

> rm `FILE0` `FILE1` ... `FILEn`

**Returns:**
- 0 on success
**Syscall:** [unlink](../../kernel/syscalls/unlink.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [rmdir](rmdir.md) | [mknod](mknod.md) | [rm](rm.md) | [stat](stat.md)
