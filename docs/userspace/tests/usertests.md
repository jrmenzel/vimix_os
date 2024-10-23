# Usertests

Main user space test suite. Based on the usertests of xv6. This version has additional tests and [builds on Linux](docs/build_instructions.md) (with a subset of tests). This way tests for Linux compatible features can be written against a known-to-work target.

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
**Tests:** [forktest](forktest.md) | [grind](grind.md) | [usertests](usertests.md) | [zombie](zombie.md) | [skill](skill.md)
