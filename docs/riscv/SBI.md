# Supervisor Binary Interface

## Overview

> The **RISC-V Supervisor Binary Interface (SBI)** is the recommended interface between:
> 
> 1. A platform-specific firmware running in M-mode and a bootloader, a hypervisor or a general-purpose OS executing in S-mode or HS-mode.
> 2. A hypervisor running in HS-mode and a bootloader or a general-purpose OS executing in VS-mode.
- https://github.com/riscv-software-src/opensbi

A hardware abstraction that can optionally run in [M-Mode](M-mode.md). If a platform provides the SBI, the OS is booted in [S-Mode](S-mode.md) instead of [M-Mode](M-mode.md) (see [boot process](../kernel/overview/boot_process.md)) and needs to use SBI calls to access [M-Mode-only](M-mode.md) functions (e.g. access to the timer [interrupts](../kernel/interrupts/interrupts.md)). If a platform only supports boot in [M-Mode](M-mode.md), a small [M-Mode](M-mode.md) runtime is provided which mimics the relevant SBI calls.


## SBI use in VIMIX

SBI support is always compiled in. The [kernel](../kernel/kernel.md) will probe the supported extensions and make use of:
- Timer extension `SBI_EXT_ID_TIME`
	- Get timer [interrupts](../kernel/interrupts/interrupts.md) from SBI as a fallback to the sstc extension based timers.
- Hart State Monitoring extension `SBI_EXT_ID_HSM`
	- Start remaining harts explicitly


## Links

**SBI Specification:**
- https://wiki.riscv.org/display/HOME/RISC-V+Technical+Specifications
- Direct link: https://drive.google.com/file/d/1LdnP5dDyc8wqLPujUqH8hIiTGKndujng/view


---
**Overview:** [RISC V](RISCV.md)
