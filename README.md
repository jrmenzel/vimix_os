# VIMIX OS Readme

VIMIX OS is a small Unix like OS which started as a fork of [xv6](https://github.com/mit-pdos/xv6-riscv) (which is a partial re-implementation of Unix version 6 for [RISC V](https://en.wikipedia.org/wiki/RISC-V) 32 and 64-bit).

See [README-xv6](docs/README-xv6.md) for original xv6 contributors.


## Quick Links

Compile and try out:
- [build_instructions](docs/build_instructions.md)
- [run_on_qemu](docs/run_on_qemu.md)


## Changes from xv6

- Reorganized code, separate headers, renamed many functions and variables, using stdint types
- Support 32-bit RISC V (in addition to 64 bit), both "bare metal" and running in a SBI environment. Inspired by a 32-bit xv6 port by Michael Schröder (https://github.com/x653/xv6-riscv-fpga/tree/main/xv6-riscv)
- The user space tries to mimics a real UNIX. Some apps can get compiled unchanged for Linux too.
