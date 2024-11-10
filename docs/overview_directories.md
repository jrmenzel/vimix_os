# Overview of the project directories


## build

Temp build files. See [build_instructions](build_instructions.md) on how to compile the system.
- `build/root` Compiled [user space](userspace/userspace.md) apps for VIMIX
- `build/root_host` Subset of the apps compiled for the host
- `build/kernel-vimix` The compiled kernel
- `build/filesystem.img` The final image of the file system for qemu


## docs

This documentation, see also: [README](../README.md)


## kernel

The VIMIX [kernel](kernel/kernel.md) contains all the code that runs in [S-mode](riscv/S-mode.md).

The sub-directory structure was inspired by Linux:
- `arch/riscv` ([RISC V](riscv/RISCV.md)) (ideally) all architecture dependent code
- `drivers` ([devices](kernel/devices/devices.md)) drivers by category
- `fs` ([file_system](kernel/file_system/file_system.md))
	- `fs/xv6fs` [xv6fs](kernel/file_system/xv6fs/xv6fs.md) is a simple file system based on the one from xv6 (which was inspired by the UNIX 6 file system)
- `include` kernel API for kernel code (e.g. drivers)
	- [user space](userspace/userspace.md) kernel API: `usr/include/sys`
- `init` ([init_overview](kernel/overview/init_overview.md)) boot and init of all subsystems
- `kernel` ([kernel](kernel/kernel.md)) primary functions of the OS like process management
- `lib` libs shared by different kernel modules
	- `lib/libfdt` [device tree](docs/misc/device_tree.md) parsing lib from https://github.com/dgibson/dtc.
- `mm` ([memory_management](kernel/mm/memory_management.md))


## tools

Tools for the development environment.
- `mkfs` The tool to create [xv6fs](kernel/file_system/xv6fs/xv6fs.md) file systems.
- `fsck.xv6fs` A simple [xv6fs](kernel/file_system/xv6fs/xv6fs.md) file system checker.
- Scripts for [debugging](debugging.md).


## usr

[user space](userspace/userspace.md) applications and libs for VIMIX.
All default apps from /usr/bin are here.
- `usr/include`: user space headers
- `usr/include/sys`: user space [kernel](kernel/kernel.md) API


---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
