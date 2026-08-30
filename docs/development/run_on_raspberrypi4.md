# How to run on Raspberry Pi 4

**Note:** While the qemu emulation works, running on real hardware still has bugs.

**Pre-requirement:**

1. Build VIMIX (see [build_instructions](development/build_instructions.md)), make sure `TARGET` is set to `arm64`.
2. Connect the Raspberry Pi 4 board to UART.
3. Start a UART terminal on the host PC (e.g. `picocom`)
4. Copy `build/boot` to a prepared SD card (flashed Pi OS so boot partition is setup with all firmware blobs).
5. Rename `kernel-vimix.img` to `kernel8.img` and `filesystem.img` to `initramfs8`.

Setup `boot/config.txt` of the Raspberry Pi, add / replace the following:

```
enable_rp1_uart=1
enable_uart=1

# until we switch to PL011
core_freq_min=500
core_freq_fixed=1

kernel=kernel-vimix.img

ramfsfile=filesystem.img
ramfsaddr=0x00a00000
auto_initramfs=0

# Run in 64-bit mode
arm_64bit=1

# Disable pull downs
gpio=22-27=np
# Enable jtag pins (i.e. GPIO22-GPIO27)
enable_jtag_gpio=1
```

(See also: https://www.raspberrypi.com/documentation/computers/config_txt.html)

**Notes:**

- In case the DTB in the firmware is out of date, VIMIX compiles in a newer version.

## Debugging via FT232H + OpenOCD + VS Code

- Requires OpenOCD.
- Enabled JTAG GPIOs in `config.txt`, see above.

### FT232H <-> Raspberry Pi 4 wiring

| JTAG signal     | FT232H pin/signal | RPi GPIO | RPi header pin |
| --------------- | ----------------- | -------- | -------------- |
| TCK             | AD0               | GPIO25   | 22             |
| TDI             | AD1               | GPIO26   | 37             |
| TDO             | AD2               | GPIO24   | 18             |
| TMS             | AD3               | GPIO27   | 13             |
| TRST (optional) | ACBUS0 / nTRST    | GPIO22   | 15             |
| GND             | GND               | GND      | any GND pin    |

### Live Debugging

Manually start:

> openocd -f tools/openocd/rpi4_ft232h.cfg

Then attach with VSCode.

---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md)
