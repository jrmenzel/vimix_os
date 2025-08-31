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

**System:** [sh](sh.md) | [which](which.md) | [mount](mount.md) | [umount](umount.md) | [shutdown](shutdown.md) 
