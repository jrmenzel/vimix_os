# How to run on OrangePi RV2

**Pre-requirement:**

1. Build VIMIX (see [build_instructions](development/build_instructions.md)), make sure `TARGET` is set to `rv64`.
2. Connect the OrangePi board to UART.
3. Start a UART terminal on the host PC (e.g. `picocom`)
4. Copy `build/boot` to a prepared SD card (or load manually, see [run_on_hardware](run_on_hardware.md)).

**Notes:**

- The DTB in the firmware is out of date, VIMIX compiles in a newer version.

## Hardware Setup

Connect a UART-to-USB adapter to the 40 pin GPIO header: When looking at the board in "landscape mode" with the USB/Ethernet/HDMI ports on the top and the GPIO header on the right, the UART is located on the top left (next to the USB-C power in).

| LEFT           | MIDDLE        | RIGHT          |
| -------------- | ------------- | -------------- |
| To GND on UART | To TX on UART | To RX on UART  |

---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md)
