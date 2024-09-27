# Ram Disk

A RAM disk is a [block device](devices.md) backed by memory. They can not be created at runtime at the moment, but are set up during build to store the root [file_system](../file_system/file_system.md) in it. This way no mass storage device driver is required to boot.

Linux boots with a ram disk (initrd) to load additional drivers, not for the full root file system.


## Embedded in the Kernel Binary

Converts the `filesystem.img` into an array and embeds it into the kernel binary.
See `RAMDISK_EMBEDDED` in `MakefileCommon.mk`.


## Loaded by a Boot Loader: initrd

Boot loaders and qemu can load a file into RAM and provide the location to the [kernel](../kernel.md) via the [device tree file](../../misc/device_tree.md). 


---
**Overview:** [kernel](../kernel.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](devices.md) | [file_system](../file_system/file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)

**Devices:** [ramdisk](ramdisk.md)
