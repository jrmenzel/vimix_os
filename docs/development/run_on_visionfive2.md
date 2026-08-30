# How to run on Visionfive 2

**Pre-requirement:**

1. Build VIMIX (see [build_instructions](development/build_instructions.md)), make sure `TARGET` is set to `rv64`.
2. Connect the Visionfive 2 board to UART.
3. Start a UART terminal on the host PC (e.g. `picocom`)
4. Copy `build/boot` to a prepared SD card (or load manually, see [run_on_hardware](run_on_hardware.md)).

**Notes:**

- The DTB in the firmware is out of date, VIMIX compiles in a newer version.

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

---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md)
