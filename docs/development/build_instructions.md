# Build Instructions

Requirements:

- qemu
- [RISC V](riscv/RISCV.md) and/or [ARM 64](../aarch64/ARM%2064.md) gcc toolchain (version 14 or later)
  - optionally clang 21 or later
- optional: clang-format version 20 or later

On Arch Linux install:

```bash
sudo pacman -S qemu-system-riscv qemu-system-riscv-firmware riscv64-elf-binutils riscv64-elf-gcc riscv64-elf-gdb riscv64-elf-newlib xxd uboot-tools
```

On Ubuntu 26.04 install the packages listed in the Dockerfile  `tools/docker/ubuntu2604`.

Build all:
> make

Run in [qemu](run_on_qemu.md):
> make qemu

Run in qemu waiting for a debugger:
> make qemu-gdb

To list additional make targets, e.g. to only rebuild the [kernel](../kernel/kernel.md):
> make help

## Build options

Make variables can be passed on the command line as `make VARIABLE=value`. The
defaults are defined in `MakefileCommon.mk` and the architecture-specific
Makefiles. Only [RISC V](riscv/RISCV.md) and [ARM 64](../aarch64/ARM%2064.md) are supported

Architecture specific setting, e.g. 32 vs 64-bit etc. can be found in `kernel/arch/<architecture>/MakefileArch.mk`. These make files define `platforms` with all relevant settings preset for either a specific emulator config or hardware device.

### Make parameters

| Parameter               | Values/default                             | Effect                                                                                                                                                                                          |
| ----------------------- | ------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `TARGET`                | `rv32`, `rv64` (default), `rv32m`, `rv64m` | Selects the RISC-V bit width and boot mode. Targets ending in `m` boot directly in [M-mode](../riscv/M-mode.md) the others boot through [SBI](../riscv/SBI.md) in [S-mode](../riscv/S-mode.md). |
| `BUILD_TYPE`            | `debug` (default), `release`               | Debug builds use `-O0` and include debug information; release builds use `-O2`.                                                                                                                 |
| `REL_WITH_DEBUG`        | `yes` (default), `no`                      | Controls whether release builds retain debug symbols and generate `.xdbg` files for better stack traces on crashes.                                                                             |
| `COMPILER`              | `gcc` (default), `clang`                   | Selects the C compiler and matching binutils. Clang uses LLVM tools.                                                                                                                            |
| `TOOLPREFIX`            | auto-detected                              | Overrides the GCC cross-toolchain prefix, for example `riscv64-unknown-elf-`.                                                                                                                   |
| `ANALYZE`               | `1` to enable                              | Enables GCC's `-fanalyzer`.                                                                                                                                                                     |
| `RV_ENABLE_EXT_C`       | `yes` (default), `no`                      | Enables or disables the RISC-V compressed instruction extension on build.                                                                                                                       |
| `RV_ENABLE_EXT_SSTC`    | `yes` (default), `no`                      | Enables the Sstc timer extension when the compiler supports it. Set it to `no` to force [SBI](../riscv/SBI.md) timers for testing.                                                              |
| `CREATE_ASSEMBLY`       | `yes` to enable                            | Generates a disassembly of the kernel.                                                                                                                                                          |
| `EXTERNAL_KERNEL_FLAGS` | empty by default                           | Appends flags or preprocessor definitions to kernel C and assembly compilation. [cicd](cicd.md) uses `-D_SHUTDOWN_ON_PANIC`. Quote the value when passing multiple flags.                       |

For example:

```bash
# 32-bit, S-mode, debug build using GCC
make TARGET=rv32 BUILD_TYPE=debug

# 64-bit, M-mode release build using GCC
make TARGET=rv64m BUILD_TYPE=release

# 64-bit release build using Clang
make TARGET=rv64 BUILD_TYPE=release COMPILER=clang

# The extra checks used by the CI GCC analyzer build
make -j$(nproc) TARGET=rv64 BUILD_TYPE=debug ANALYZE=1 \
    EXTERNAL_KERNEL_FLAGS=-D_SHUTDOWN_ON_PANIC
```

The GitLab CI build matrix covers debug and release builds for all four GCC
targets, plus debug and release builds for the `rv32` and `rv64` Clang targets.
Run `make clean` before rebuilding an existing target with different
compiler, extension, or flag settings.

The emulator-related parameters are:

| Parameter | Values/default | Effect |
| --- | --- | --- |
| `EMU` | See table below; derived from `TARGET` by default | Selects the QEMU/Spike boot environment, bit width, and root-filesystem transport. |
| `CPUS` | `4` | Number of emulated CPUs. |
| `MEMORY_SIZE` | `64` | Emulated memory size in MiB. |
| `GDB_PORT` | `26000` | TCP port used by `qemu-gdb` and generated GDB configuration. |

| `EMU` | Boot mode | Root filesystem | Width |
| --- | --- | --- | --- |
| `sbi32`, `sbi64` | SBI/S-mode | virtio disk | 32/64-bit |
| `sbi-rdisk32`, `sbi-rdisk64` | SBI/S-mode | boot-loader initrd | 32/64-bit |
| `m32`, `m64` | M-mode | virtio disk | 32/64-bit |
| `m-rdisk32`, `m-rdisk64` | M-mode | boot-loader initrd | 32/64-bit |

The default emulator is `sbi32`/`sbi64` for `rv32`/`rv64`, and
`m-rdisk32`/`m-rdisk64` for `rv32m`/`rv64m`. `TARGET` and `EMU` should use the
same bit width and a compatible boot mode. Examples:

```bash
make TARGET=rv32 qemu
make TARGET=rv64m EMU=m-rdisk64 spike
make TARGET=rv64 EMU=sbi64 CPUS=8 MEMORY_SIZE=128 qemu
make TARGET=rv64 GDB_PORT=26001 qemu-gdb
```

### 32- vs 64-bit

For RISC V either 32-bit or 64-bit target can get selected based on the target. 64-bit [kernel](kernel/kernel.md) can only run 64-bit [applications](userspace/userspace.md). Both versions have the same features.

### RISC V Extensions

The use of the RISC V compressed instruction extension can be disabled by setting `RV_ENABLE_EXT_C=no`.

When `RV_ENABLE_EXT_SSTC` is set, the timer will be based on this extension (if available) instead of using the [SBI](riscv/SBI.md) timer.

### SBI

VIMIX can run bare-metal (booting in [M-mode](riscv/M-mode.md)) or via an [SBI](riscv/SBI.md) compatible environment in [S-mode](riscv/S-mode.md). Select the mode with `TARGET`, as described in the make-parameter table above.

### Root Filesystem

The root [file system](kernel/file_system/file_system.md) can be on a [ramdisk](kernel/devices/ramdisk.md), either embedded in the kernel binary or loaded by the boot loader from a file. On [qemu](run_on_qemu.md) it can also be a virtio [device](kernel/devices/devices.md). See make file variables `VIRTIO_DISK`, `RAMDISK_EMBEDDED` and `RAMDISK_BOOTLOADER`.

### Kernel parameters

The file `param.h` sets various system values like the maximum supported CPUs or processes. It also contains debug switches which enable additional runtime tests.

## Compile apps for the host

Some [user space](userspace/userspace.md) apps compile on the host (tested on Linux).

> make host

The binaries end up in `build_host/root/usr/bin`. See `.vscode/launch.json` on how to debug them running on the host.

---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md)
