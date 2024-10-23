# grind

From xv6.
Run random system calls in parallel forever. Uses 2 stating processes by default, can this can be set as a command line parameter. The starting processes will also fork sub tasks.

The intention is to find dead-locks in the [file system](docs/kernel/file_system/file_system.md). Let it run an watch for the periodic output indicating a still running fork. If the kernel is hung by a dead-lock, the console shortcuts might still work to inspect the process state, see [debugging](docs/debugging.md).

Run
> grind `optional fork count`

**Returns:**
- won't return

---
**Up:** [user space](../userspace.md)
**Tests:** [forktest](forktest.md) | [grind](grind.md) | [usertests](usertests.md) | [zombie](zombie.md) | [skill](skill.md)
