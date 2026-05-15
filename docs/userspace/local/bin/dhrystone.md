# dhrystone

An old benchmark measuring integer performance. This version was taken from https://github.com/sifive/benchmark-dhrystone/tree/master and minimally adapted to build for VIMIX (e.g. not using floats in the score calculation). It's based on version 2.1.

Relies on [clock_gettime](../../../kernel/syscalls/clock_gettime.md) to measure performance, so it needs to run for a few seconds. On an emulator a low number of million runs is normally sufficient. Dhrystone will complain if it didn't run long enough.

> dhrystone <millions_of_runs>

See also: https://en.wikipedia.org/wiki/Dhrystone

## Changes to the original

- Added a command line to set number of runs.

---
**Up:** [user space](../../userspace.md)
