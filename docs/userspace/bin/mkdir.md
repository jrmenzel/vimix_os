# mkdir - make directories

Make one or more directories. Does not stop after one directory creation failed.

> mkdir `DIR0` `DIR1` ... `DIRn`

**Returns:**
- 0 on success
- 1 on error (no parameter given or some of the directories failed to get created)
**Syscall:** [mkdir](../../kernel/syscalls/mkdir.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [cp](cp.md) | [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mknod](mknod.md) | [rm](rm.md) | [rmdir](rmdir.md) | [stat](stat.md) | [statvfs](statvfs.md)
