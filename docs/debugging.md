# Debugging Hints

**Pre-requirement:** 
1. Build the system with debug symbols (see `BUILD_TYPE` in `MakefileCommon.mk`, also see [build_instructions](build_instructions.md)).


## Debugging on emulators

The VIMIX kernel and user space apps can be debugged when running on [qemu](run_on_qemu.md) or [spike](run_on_spike.md), see there for setup details. Debugging on qemu is recommended (faster and more convenient).


## Disassembly

Sometimes it can be helpful to look at the generated assembly code. `tools/disassemble.sh` is a small wrapper around `objdump` for this. It can disassemble starting at an address defined by the ELF file (or functions by name). As binaries are mapped to these locations, program counter addresses (e.g. from exceptions, call stacks) can be used as disassembly starting points to find the source location.


## Console build-in process list

The console captures some key combinations and prints debug output. Press `CTRL + H` for a list.


---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
