# The Shell


The shell uses [fork](../../kernel/syscalls/fork.md) and [execv](../../kernel/syscalls/execv.md) to start user applications.
Normally it gets started by [init](init.md).

Searches for commands in `/usr/bin` and `/usr/local/bin`.

See also [life_cycle_user_application](../../kernel/overview/life_cycle_user_application.md).

---
**Up:** [user space](../userspace.md)

**System:** [meminfo](meminfo.md) | [mount](mount.md) | [shutdown](shutdown.md) | [sh](sh.md) | [umount](umount.md) | [which](which.md)
