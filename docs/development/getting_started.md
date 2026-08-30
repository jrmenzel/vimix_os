# Getting Started

See [build instructions](build_instructions.md) for:

- Software requirements to compile VIMIX.
- How to configure and build VIMIX.

Run VIMIX on one of the [supported platforms](../misc/architectures.md):

- On [RISCV](../riscv/RISCV.md):
    - The fast [qemu](run_on_qemu.md) emulator.
    - The slower but very accurate [Spike](run_on_spike.md) simulator.
    - The [VisionFive2](run_on_visionfive2.md) and [OrangePI RV2](run_on_orangepi.md) single board computers.
- On [ARM 64](../aarch64/ARM%2064.md):
    - The fast [qemu](run_on_qemu.md) emulator, optionally with KVM virtualization on ARM hosts.

Useful pointers:

- [Overview of the directory layout](overview_directories.md).
- How [automated tests](automated_tests.md) work.
- Some [debugging](debugging.md) hints.

---
**Up:** [README](../../README.md)

[build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md)
