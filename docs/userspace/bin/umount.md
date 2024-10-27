# umount

Unmounts a [file_system](../../kernel/file_system/file_system.md).

> umount `target dir`

- `target dir` must be a directory and and a mount point

**Returns:**
- 0 on success and sets errno
**Syscall:** [umount](../../kernel/syscalls/umount.md)

---
**Up:** [user space](../userspace.md)

**System:** [mount](mount.md) | [umount](umount.md) | [shutdown](shutdown.md) 
