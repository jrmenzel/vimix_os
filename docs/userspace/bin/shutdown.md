# shutdown

Halts or shuts down the OS.

Both halt:
> shutdown

> shutdown -h

Reboots:
> shutdown -r

**Returns:**
- ideally never, negative values on error
**Syscall:** [reboot](../../kernel/syscalls/reboot.md)

---
**Up:** [user space](../userspace.md)

**System:** [meminfo](meminfo.md) | [mount](mount.md) | [shutdown](shutdown.md) | [sh](sh.md) | [umount](umount.md) | [which](which.md)
