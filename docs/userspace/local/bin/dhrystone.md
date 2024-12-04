# dhrystone

An old benchmark measuring integer performance. This version was taken from https://github.com/sifive/benchmark-dhrystone/tree/master and minimally adapted to build for VIMIX (e.g. not using floats in the score calculation). It's based on version 2.1.

Relies on [get_time](../../../kernel/syscalls/get_time.md) to measure performance, so it needs to run for a few seconds. Set `DHRY_ITERS` to the desired iterations.

See also: https://en.wikipedia.org/wiki/Dhrystone


---
**Up:** [user space](../../userspace.md)
