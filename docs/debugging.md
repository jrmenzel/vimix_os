# Debugging Hints

Pre-requirement: Build the system with debug symbols (see `BUILD_TYPE` in `MakefileCommon.mk`, also see [build_instructions](build_instructions.md)).


## Qemu

Start qemu with
> make qemu-gdb

qemu will wait for a debugger to attach, plain `gdb` works (you'll need the port number, which gets set in the `Makefile`, see `GDB_PORT`).

Alternatively use `VSCode`, a `launch.json` is provided with various settings:
- 32-bit vs 64-bit must be selected matching the compile target.
- Either [user space](userspace/userspace.md) applications or the [kernel](kernel/kernel.md) can be debugged, but not both at the same time (read: no stepping through a [syscall](kernel/syscalls/syscalls.md) into the kernel).
- Adjust the presets for user space apps that don't have one.

**Note:** In both cases, `gdb` or `VSCode`, qemu must be started manually.


## Disassembly

Sometimes it can be helpful to look at the generated assembly code. `tools/disassemble.sh` is a small wrapper around `objdump` for this. It can disassemble starting at an address defined by the ELF file. As binaries are mapped to these locations, program counter addresses (e.g. from exceptions) can be used as disassembly starting points to find the source location.


---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
