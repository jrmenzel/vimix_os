# mount

Mounts a [file_system](../../kernel/file_system/file_system.md).

> mount -t `fstype` `source` `target dir`

- `fstype` must be a supported file system type, e.g. `vimixfs`
- `source` must be a path to a [block device](../../kernel/devices/devices.md)
- `target dir` must be a directory and will be the mount point

**Returns:**
- 0 on success and sets errno
**Syscall:** [mount](../../kernel/syscalls/mount.md)

---
**Up:** [user space](../userspace.md)

**System:** [fsinfo](fsinfo.md) | [meminfo](meminfo.md) | [mount](mount.md) | [shutdown](shutdown.md) | [sh](sh.md) | [umount](umount.md) | [which](which.md)
