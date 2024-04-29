# Build Instructions

Requirements: qemu, 32- or 64-bit RISC V gcc toolchain

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

In `MakefileCommon.mk` some options can get selected, e.g. debug vs. release build, 32 vs 64-bit etc.

### 32- vs 64-bit

For RISC V either 32-bit or 64-bit target can get selected in `MakefileCommon.mk`. 64-bit kernels can only run 64-bit applications. Both versions have the same features. `make qemu` will automatically run the right variant of qemu, but in VSCode the matching debug targets must get selected manually. For VSCode two settings for syntax highlighting are provided: one per bit width. They set the correct defines which are defined by the Makefiles.


### SBI

VIMIX can run bare-metal (booting in **M Mode**) or via a SBI compatible environment in **S Mode**. See `SBI_SUPPORT` in `MakefileCommon.mk`.


### Kernel parameters

The file `param.h` sets various system values like the maximum supported CPUs or processes. It also contains debug switches which enable additional runtime tests.

---
[README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md)