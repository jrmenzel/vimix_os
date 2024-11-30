# How to run on Visionfive 2

**Pre-requirement:** 
1. Build VIMIX (see [build_instructions](build_instructions.md)), make sure `PLATFORM` in `MakefileArch.mk` is set to `visionfive2`.
2. Connect the Visionfive 2 board to UART.
3. Start a UART terminal on the host PC (e.g. `picocom`)


## Loading VIMIX

Boot the Visionfive 2 board and stop the default boot sequence. The commands below are U-Boot commands to load the VIMIX binary.

The memory starts at `0x40000000`, the firmware is also mapped here. The usable memory (and where the kernel gets mapped to) starts at `0x40200000`. Before `bootelf` loads the kernel to that address, the elf file has to be loaded into some higher memory location. `0x50000000` works fine.


### From UART

> loady 0x50000000

CTRL+A CTRL+S:
> ./vimix_os/build/kernel-vimix
> bootelf 0x50000000


### From SD Card

Copy `build/kernel-vimix` to the root of the SD card. Reboot the board after inserting the SD card.

> load mmc 1 0x50000000 kernel-vimix
> bootelf 0x50000000



---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md) |  [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
