# How to run on Spike

**Pre-requirement:** 
1. Install Spike (https://github.com/riscv-software-src/riscv-isa-sim).
	1. Note that Spike can be build with [SBI](riscv/SBI.md) support, see https://github.com/riscv-software-src/opensbi/blob/master/docs/platform/spike.md . This requires to build OpenSBI (e.g. `make CROSS_COMPILE=riscv64-linux-gnu- PLATFORM=generic`) from https://github.com/riscv-software-src/opensbi/tree/master . Configure [VIMIX](build_instructions.md) accordingly.
	2. Set the correct Spike binary in the Makefile.
2. Optional: Install OpenOCD (for debugging)
3. Configure VIMIX:
	1. Set `PLATFORM` in `MakefileArch.mk` to `spike`. This will set:
		1. to use a [ramdisk](kernel/devices/ramdisk.md) (initrd or embedded)
		2. 32-bit build (64-bit defaults to a SV57 MMU, VIMIX needs a SV39 MMU. Spike can be modified to support SV39: Look for SV57 in `riscv/dts.cc` and `riscv/processor.cc`)
	2. Clean and rebuild after switching the `PLATFORM`!


Run
> make spike


## Debugging on Spike with VSCode

1. Run Spike
> make spike-gdb

2. Run OpenOCD
> openocd -f spike.cfg

3. Attach debugger from VSCode

**Known issues:**
Spike starts executing without waiting for the debugger. The command line option `-H` prevents this, but also somehow generated exceptions during execution.


---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md) |  [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
