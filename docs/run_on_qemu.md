# How to run on qemu

Install qemu for RISC V 64-bit or 32-bit. Build VIMIX (see [build_instructions](build_instructions.md)).

Run 
> make qemu

Or run manually:
> qemu-system-riscv64 -machine virt -bios none -kernel build/kernel-vimix -m 128M -smp 4 -nographic -global virtio-mmio.force-legacy=false -drive file=build/filesystem.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0


## Debug on qemu with VSCode

Run qemu
> make qemu-gdb

Attach debugger from VSCode (select matching run target for the kernel or a user space program).

---
**Up:** [README](../README.md)

[build_instructions](build_instructions.md) | [debugging](debugging.md) | [run_on_qemu](run_on_qemu.md) | [overview_directories](overview_directories.md) | [architectures](architectures.md) | [kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
