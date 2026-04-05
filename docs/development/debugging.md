# Debugging Hints

**Pre-requirement:** 
1. Build the system with debug symbols (see `BUILD_TYPE` in `MakefileCommon.mk`, also see [build_instructions](development/build_instructions.md)).

> make BUILD_TYPE=debug

## Debugging on emulators

The VIMIX kernel and user space apps can be debugged when running on [qemu](run_on_qemu.md) or [spike](run_on_spike.md), see there for setup details. Debugging on qemu is recommended (faster and more convenient).


## Disassembly

Sometimes it can be helpful to look at the generated assembly code. `tools/disassemble.sh` is a small wrapper around `objdump` for this. It can disassemble starting at an address defined by the ELF file (or functions by name). As binaries are mapped to these locations, program counter addresses (e.g. from exceptions, call stacks) can be used as disassembly starting points to find the source location.


## Console build-in process list

The console captures some key combinations and prints debug output. Press `CTRL + H` for a list.


## Call stack in kernel panics

Kernel panics can print the call stack of the offending code which might include source information. When [building](development/build_instructions.md) VIMIX for debug a tool extracts the source file and line information from the kernel and generates a simple lookup table (see`lib/xdbg`). The kernel tries to load this file before creating the first process (must run in a process to allow sleep on disk IO). 


---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md) 
