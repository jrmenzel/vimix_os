# How to run on Hardware

**Pre-requirement:**

1. Build VIMIX (see [build_instructions](development/build_instructions.md)), make sure `TARGET` is set to `rv64`.
2. Connect the board to UART.
3. Start a UART terminal on the host PC (e.g. `picocom`)

## Hardware Setup

Connect a UART-to-USB adapter to the UART GPIO pins of the respektive board.

## Console Emulator

Start a console emulator:
> minicom -D /dev/ttyUSB0 -b 115200

or:
> picocom -b 115200 -r -l /dev/ttyUSB0

To upload data via UART use `picocom`:
> picocom -b 115200 -r -l /dev/ttyUSB0 --send-cmd "lrzsz-sx --ymodem"

## Loading VIMIX

There are multiple ways to boot VIMIX:

- Easiest: Use a boot script to automatically boot from SD card (on supported platforms)
- Use U-Boot to load the files from UART
- Use U-Boot to load the files from the SD card

### Automatically via U-Boot

Create an SD card with a Ext2 file system, add a `boot` directory and copy all of `build/boot` to the card.

U-Boot will check on boot if the SD card has a `boot/boot.scr` script and executes that. It contains U-Boot commands similar to the manual loading below (source is in `boot/boot.cmd`).

### Manually from U-Boot

Boot the board and stop the default boot sequence. The commands below are U-Boot commands to load the VIMIX binary. It assumes the file system is an embedded [ramdisk](kernel/devices/ramdisk.md). The [device tree file](misc/device_tree.md) in the ROM of the board may not up to date, in that case a newer embedded DTB can be chosen at boot time. Ignoring the firmware provided DTB is also an option.

Examples below are from Visionfive 2 where the memory starts at `0x40000000`, adjust according to the target board. The usable memory (and where the kernel gets mapped to) starts below the firmware, here at `0x40200000`. Before `bootelf` loads the kernel to that address, the elf file has to be loaded into some higher memory location. `0x50000000` works fine.

#### From UART

```
StarFive # loady 0x50000000
```

CTRL+A CTRL+S:

```
StarFive # ./vimix_os/build/kernel-vimix
StarFive # bootelf 0x50000000
```

#### From SD Card

Create an SD card with a Ext2 file system. Copy `build/boot/kernel-vimix` to the root of the SD card. Reboot the board after inserting the SD card.

```
StarFive # load mmc 1 0x50000000 kernel-vimix
StarFive # bootelf 0x50000000
```

U-Boot commands can be concatenated:

> load mmc 1 0x50000000 kernel-vimix && bootelf 0x50000000

---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md)
