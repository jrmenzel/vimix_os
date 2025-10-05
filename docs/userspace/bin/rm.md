# rm - remove

Deletes [files](../../kernel/file_system/file.md), but not [directories](../../kernel/file_system/directory.md) (use [rmdir](rmdir.md)), unless when used with `-r`.

> rm `FILE0` `FILE1` ... `FILEn`
> rm -r `dir`

**Returns:**
- 0 on success
**Syscall:** [unlink](../../kernel/syscalls/unlink.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [cp](cp.md) | [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mknod](mknod.md) | [rm](rm.md) | [rmdir](rmdir.md) | [stat](stat.md) | [statvfs](statvfs.md)
