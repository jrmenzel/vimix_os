# Device Tree

A device tree is a hardware description passed on from the boot loader to the [kernel](../kernel/kernel.md).
Spec: https://www.devicetree.org/

Parsing is done with libfdt from https://github.com/dgibson/dtc (see `kernel/lib/libfdt/readme.txt` for details).

The device tree gets parsed early in the boot process in [main()](../kernel/overview/init_overview.md). This defines the [memory_map_kernel](../kernel/mm/memory_map_kernel.md) (total RAM, mapped devices) and a list of [devices](../kernel/devices/devices.md) to init.


## Print Device Tree

To dump the device tree on [qemu](../run_on_qemu.md) run:

> make qemu-dump-tree

This will generate `tree.dtb` and `tree.dts`. Requires the dtc app.


On a real device like the [VisionFive 2](../run_on_visionfive2.md) enter U-Boot and print the device tree with:

```
StarFive # fdt print /
```

---
[kernel](../kernel/kernel.md) | [userspace](../userspace/userspace.md)
