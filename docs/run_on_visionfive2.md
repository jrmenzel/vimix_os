# How to run on Visionfive 2

**Pre-requirement:** 
1. Build VIMIX (see [build_instructions](build_instructions.md)), make sure `PLATFORM` in `MakefileArch.mk` is set to `visionfive2`.
2. Connect the Visionfive 2 board to UART.
3. Start a UART terminal on the host PC (e.g. `picocom`)

## Hardware Setup

Connect a UART-to-USB adapter to the 40 pin GPIO header: When looking at the board in "portrait mode" with the USB/Ethernet/HDMI ports on the left, the GPIO header is on the top right. 

| LEFT         | RIGTH        |                |
| ------------ | ------------ | -------------- |
| 3.3V         | 5V           |                |
| I2C SDA      | 5V           |                |
| I2C SCL      | GND          | To GND on UART |
| GPIO 55      | UART TX      | To RX on UART  |
| GND          | UART RX      | To TX on UART  |
| GPIO 42      | GPIO 38      |                |
| 14 more pins | 14 more pins |                |

See https://wiki.52pi.com/index.php?title=ER-0043 for more details.


## Console Emulator

Start a console emulator:
> minicom -D /dev/ttyUSB0 -b 115200

or:
> picocom -b 115200 -r -l /dev/ttyUSB0

To upload data via UART use `picocom`:
> picocom -b 115200 -r -l /dev/ttyUSB0 --send-cmd "lrzsz-sx --ymodem"


## Loading VIMIX

There are multiple ways to boot VIMIX:
- Easiest: Use a boot script to automatically boot from SD card
- Use U-Boot to load the files from UART
- Use U-Boot to load the files from the SD card


### Automatically via U-Boot

Create an SD card with a Ext2 file system, add a `boot` directory and copy all of `build/boot` to the card.

U-Boot will check on boot if the SD card has a `boot/boot.scr` script and executes that. It contains U-Boot commands similar to the manual loading below (source is in `boot/boot.cmd`).


### Manually from U-Boot

Boot the Visionfive 2 board and stop the default boot sequence. The commands below are U-Boot commands to load the VIMIX binary. It assumes the file system is an embedded [ramdisk](kernel/devices/ramdisk.md). The [device tree file](misc/device_tree.md) in the ROM of the board is not up to date, so better also embed the provided device tree into the kernel image.

The memory starts at `0x40000000`, the firmware is also mapped here. The usable memory (and where the kernel gets mapped to) starts at `0x40200000`. Before `bootelf` loads the kernel to that address, the elf file has to be loaded into some higher memory location. `0x50000000` works fine.


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
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md) |  [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
