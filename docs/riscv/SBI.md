# Supervisor Binary Interface

## Overview

> The **RISC-V Supervisor Binary Interface (SBI)** is the recommended interface between:
> 
> 1. A platform-specific firmware running in M-mode and a bootloader, a hypervisor or a general-purpose OS executing in S-mode or HS-mode.
> 2. A hypervisor running in HS-mode and a bootloader or a general-purpose OS executing in VS-mode.
- https://github.com/riscv-software-src/opensbi

A hardware abstraction that can optionally run in M-Mode. If a platform provides the SBI, the OS is booted in S-Mode instead of M-Mode (see [boot_process](../kernel/overview/boot_process.md)) and needs to use SBI calls to access M-Mode-only functions (e.g. access to the timer [interrupts](../kernel/interrupts/interrupts.md)).


## SBI use in VIMIX

When SBI support is compiled in, the [kernel](../kernel/kernel.md) will probe the supported extensions and make use of:
- Timer extension `SBI_EXT_ID_TIME`
	- Get timer [interrupts](../kernel/interrupts/interrupts.md) from SBI instead of [PLIC](PLIC.md) in [M-mode](M-mode.md).
- Hart State Monitoring extension `SBI_EXT_ID_HSM`
	- Start remaining harts explicitly (without [SBI](SBI.md) all harts would just boot the same kernel code)


## Links

**SBI Specification:**
- https://wiki.riscv.org/display/HOME/RISC-V+Technical+Specifications
- Direct link: https://drive.google.com/file/d/1LdnP5dDyc8wqLPujUqH8hIiTGKndujng/view


---
**Overview:** [RISC V](RISCV.md)
