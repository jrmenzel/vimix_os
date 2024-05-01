# Usertests

Main user space test suite. From xv6.

> usertests `options` OR `testname`

Without a `testname`, all tests are run.

**Options:**

`-q` Only run the quick tests
`-c` Run tests in an infinite loop until one fails
`-C` Run tests in an infinite loop, ignore failures


**Returns:**
- 0 on success

---
**Up:** [user space](../userspace.md)
**Tests:** [forktest](forktest.md) | [grind](grind.md) | [usertests](usertests.md) | [zombie](zombie.md)
