# Build Instructions

Requirements: qemu, 64-bit RISC V gcc toolchain

on Arch Linux install for 64-bit:
> sudo pacman -S qemu-system-riscv qemu-system-riscv-firmware riscv64-elf-binutils riscv64-elf-gcc riscv64-elf-gdb riscv64-elf-newlib

Build all:
> make

Build only the kernel:
> make kernel

Run in qemu:
> make qemu

Run in qemu waiting for a debugger:
> make qemu-gdb


## Build options

In `MakefileCommon.mk` some options can get selected, e.g. debug vs. release build.


### Kernel parameters

The file `param.h` sets various system values like the maximum supported CPUs or processes. It also contains debug switches which enable additional runtime tests.

---
[README](../README.md)

[build_instructions](build_instructions.md) | [run_on_qemu](run_on_qemu.md)
