# su - switch user

Switches to the given user (sets [user / group IDs](../../kernel/security/user_group_id.md)) and executes a new [sh](sh.md).

> su user

To change the user ID via [setuid](../../kernel/syscalls/setuid.md), the [process](../../kernel/processes/processes.md) must run as `root`. To allow other users to execute `su`, the executable has the [set user ID bit](../../kernel/security/mode.md) set. 


---
**Up:** [user space](../userspace.md)

**System:** [fsbench](fsbench.md) | [fsinfo](fsinfo.md) | [meminfo](meminfo.md) | [mount](mount.md) | [shutdown](shutdown.md) | [sh](sh.md) | [su](su.md) | [umount](umount.md) | [which](which.md)
