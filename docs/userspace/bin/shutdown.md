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

**Misc:** [cat](cat.md) | [echo](echo.md) | [grep](grep.md) | [wc](wc.md) | [shutdown](shutdown.md) | [date](date.md) | [sleep](sleep.md)
