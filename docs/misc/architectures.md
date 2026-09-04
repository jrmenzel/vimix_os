# Supported Architectures and Arch Specifics

## VIMIX feature parity

Some device drivers are architecture specific and naturally are only available on the respective platform (e.g. the [SBI](../riscv/SBI.md) driver is [RISCV](../riscv/RISCV.md) only).

Real known limits exposed by one architecture but not the other:

- Maximum of supported CPUs is 8 on [ARM 64](../aarch64/ARM%2064.md) based on a limitation of the supported interrupt controller.
- qemus `virtio` "harddrive" is only working on [RISC V](../riscv/RISCV.md).

## RISC V

VIMIX supports [RISC V](riscv/RISCV.md) 32- and 64-bit. It can boot in [M-Mode](riscv/M-mode.md) (providing a minimal subset of [SBI](riscv/SBI.md) ecalls) or on [SBI](riscv/SBI.md) in [S-Mode](riscv/S-mode.md).

### 32-bit RISC V

64-bit integer math on a 32-bit CPU requires software implementations of division etc. This is why only the 32-bit [kernel](../kernel/kernel.md) and [userspace](../../userspace/userspace.md) require `kernel/lib/div64.c`. Handling of 64-bit time values on 32-bit systems makes this necessary (see [clock_gettime](kernel/syscalls/clock_gettime.md)).

### 64-bit RISC V

Only 64-bit [user space](userspace/userspace.md) is supported.
To support 32-bit applications on a 64-bit [kernel](kernel/kernel.md) at least these changes would be required:

- Mark processes as 32- vs 64-bit
- Limit virtual memory addresses of 32-bit processes to 4GB.
- Switch CPU mode to execute 32-bit code before switching to process
- Switch back in interrupt
- Only store 32-bit registers at context switch (sign extend?)

Only sv39 [memory_management](kernel/mm/memory_management.md) is supported.

### RISC V Platforms

- [qemu emulator](../development/run_on_qemu.md)
- [spike emulator](../development/run_on_spike.md)
- [Visionfive 2 development board](../development/run_on_visionfive2.md)
- [OrangePI RV2 development board](../development/run_on_orangepi.md)

## ARM 64

### ARM 64 Platforms

- [qemu virt device](../development/run_on_qemu.md)
- [Raspberry Pi 4](../development/run_on_raspberrypi4.md)
  - Does not support Raspberry Pi 3, as it requires a different interrupt controller.

---
**Up:** [README](../../README.md)

**Achitectures:** [ARM 64](../aarch64/ARM%2064.md) | [RISC V](../riscv/RISCV.md)

[build_instructions](development/build_instructions.md) | [debugging](../development/debugging.md) | [run_on_qemu](../development/run_on_qemu.md) | [run_on_spike](../development/run_on_spike.md) | [run_on_visionfive2](../development/run_on_visionfive2.md) |  [overview_directories](../development/overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
