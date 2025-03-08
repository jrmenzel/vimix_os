# How to run on Spike

**Pre-requirement:** 
1. Install Spike (https://github.com/riscv-software-src/riscv-isa-sim).
	1. Note that Spike can be build with [SBI](riscv/SBI.md) support, see https://github.com/riscv-software-src/opensbi/blob/master/docs/platform/spike.md . This requires to build OpenSBI (e.g. `make CROSS_COMPILE=riscv64-linux-gnu- PLATFORM=generic`) from https://github.com/riscv-software-src/opensbi/tree/master . 
2. Optional: Install OpenOCD (for debugging)
3. Configure VIMIX:
	1. Set `PLATFORM` in `MakefileArch.mk` to `spike32` or `spike64`. This will set:
		1. to use a [ramdisk](kernel/devices/ramdisk.md) (initrd or embedded)
	2. Clean and rebuild after switching the `PLATFORM`!


Run
> make spike


## Debugging on Spike with VSCode

1. Run Spike
> make spike-gdb

2. Run OpenOCD
> openocd -f tools/openocd/spike.cfg

3. Attach debugger from VSCode

Set the Spike binary in `Makefile` when not calling Spike via the default PATH or the self compiled version created by the `tools/build_spike.sh` script. Check that Spike supports a `ns16550` UART, [PLIC](riscv/PLIC.md) and [CLINT](riscv/CLINT.md). To do so check the [device_tree](misc/device_tree.md) via:

> make PLATFORM=spike spike-dump-tree


---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md) |  [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
