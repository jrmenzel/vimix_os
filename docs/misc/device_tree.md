# Device Tree

A device tree is a hardware description passed on from the bootloader to the [kernel](../kernel/kernel.md).
Spec: https://www.devicetree.org/

Parsing is done with libfdt from https://github.com/dgibson/dtc (see `kernel/lib/libfdt/readme.txt` for details).

The device tree gets parsed early in the boot process in [main()](../kernel/overview/init_overview.md). This defines the [memory_map_kernel](../kernel/mm/memory_map_kernel.md) (total RAM, mapped devices) and a list of [devices](../kernel/devices/devices.md) to init (it also tries to find init order dependencies).

The device tree address is passed into the init function of [devices](kernel/devices/devices.md) to allow them to query device specific options.

Note that all values returned from the device tree are 32 bit aligned. When casting return values into 64 bit ints use `__attribute__((__aligned__(4)))` (or the predefined typedefs `dtb_aligned_uint64_t`/`dtb_aligned_int64_t`). Otherwise unaligned reads can trigger an exception on architectures which don't support them.


## Print Device Tree

### qemu

To dump the device tree of [qemu](../run_on_qemu.md) run:

> make PLATFORM=qemu qemu-dump-tree


To dump the device tree of [Spike](../run_on_spike.md) run:

> make PLATFORM=spike spike-dump-tree

Both will generate `*.dtb` and `*.dts` files named after the current platform. Requires the dtc app.


### VisionFive 2

On a real device like the [VisionFive 2](../run_on_visionfive2.md) enter U-Boot and print the device tree with:

```
StarFive # fdt print /
```



---
[kernel](../kernel/kernel.md) | [userspace](../userspace/userspace.md)
