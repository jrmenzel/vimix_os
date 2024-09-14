# How to run on qemu

**Pre-requirement:** 
1. Install qemu for RISC V 64-bit or 32-bit. 
2. Build VIMIX (see [build_instructions](build_instructions.md)), make sure `PLATFORM` in `MakefileCommon.mk` is set to `qemu`.


Run 
> make qemu

Or run manually:
> qemu-system-riscv64 -machine virt -bios none -kernel build/kernel-vimix -m 128M -smp 4 -nographic -global virtio-mmio.force-legacy=false -drive file=build/filesystem.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0


## Debugging on qemu with gdb or VSCode

Start qemu with
> make qemu-gdb

qemu will wait for a debugger to attach, plain `gdb` works (you'll need the port number, which gets set in the `Makefile`, see `GDB_PORT`).

Alternatively use `VSCode`, a `launch.json` is provided with various settings:
- 32-bit vs 64-bit must be selected matching the compile target.
- Either [user space](userspace/userspace.md) applications or the [kernel](kernel/kernel.md) can be debugged, but not both at the same time (read: no stepping through a [syscall](kernel/syscalls/syscalls.md) into the kernel).
- Adjust the presets for user space apps that don't have one.

**Note:** In both cases, `gdb` or `VSCode`, qemu must be started manually.

---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
