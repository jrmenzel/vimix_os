# Build Instructions

Requirements: qemu, 32- or 64-bit [RISC V](riscv/RISCV.md) gcc toolchain

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

In `MakefileCommon.mk` some options can get selected, e.g. debug vs. release build, and the architecture to build for. Currently only [RISC V](riscv/RISCV.md) is supported. 

Architecture specific setting, e.g. 32 vs 64-bit etc. can be found in `kernel/arch/<architecture>/MakefileArch.mk`.


### 32- vs 64-bit

For RISC V either 32-bit or 64-bit target can get selected in `MakefileArch.mk`. 64-bit [kernel](kernel/kernel.md) can only run 64-bit [applications](userspace/userspace.md). Both versions have the same features. `make qemu` will automatically run the right variant of qemu, but in VSCode the matching debug targets must get selected manually. For VSCode two settings for syntax highlighting are provided: one per bit width. They set the correct defines which are defined by the Makefiles.


### RISC V Extensions

The use of the RISC V compressed instruction extension can be disabled by setting `RV_ENABLE_EXT_C=no`.

When `RV_ENABLE_EXT_SSTC` is set, the timer will be based on this extension instead of using the [SBI](riscv/SBI.md) or [M-mode](riscv/M-mode.md) timers.


### SBI

VIMIX can run bare-metal (booting in [M-mode](riscv/M-mode.md)) or via a [SBI](riscv/SBI.md) compatible environment in [S-mode](riscv/S-mode.md). See `SBI_SUPPORT` in `MakefileCommon.mk`.


### Root Filesystem

The root [file system](kernel/file_system/file_system.md) can be on a [ramdisk](kernel/devices/ramdisk.md), either embedded in the kernel binary or loaded by the boot loader from a file. On [qemu](run_on_qemu.md) it can also be a virtio [device](kernel/devices/devices.md). See make file variables `VIRTIO_DISK`, `RAMDISK_EMBEDDED` and `RAMDISK_BOOTLOADER`.


### Kernel parameters

The file `param.h` sets various system values like the maximum supported CPUs or processes. It also contains debug switches which enable additional runtime tests.


## Compile apps for the host

Some [user space](userspace/userspace.md) apps compile on the host (tested on Linux).

> make host

The binaries end up in `build_host/root/usr/bin`. See `.vscode/launch.json` on how to debug them running on the host.


---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
