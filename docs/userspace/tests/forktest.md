# forktest

From xv6.
Test that [fork](../../kernel/syscalls/fork.md) fails gracefully.
It's a tiny executable (and not part of [usertests](usertests.md)) so that the limit of the supported [processes](../../kernel/processes/processes.md) can be reached without running out of memory.

Run
> forktest

**Returns:**
- 0 on success

---
**Up:** [user space](../userspace.md)
**Tests:** [forktest](forktest.md) | [grind](grind.md) | [usertests](usertests.md) | [zombie](zombie.md) | [skill](skill.md)
