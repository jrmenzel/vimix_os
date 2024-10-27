# VIMIX OS Readme

VIMIX OS is a small Unix like OS which started as a fork of [xv6](https://github.com/mit-pdos/xv6-riscv) (which is a partial re-implementation of Unix version 6 for [RISC V](https://en.wikipedia.org/wiki/RISC-V) 32 and 64-bit).

See [README-xv6](docs/README-xv6.md) for original xv6 contributors.


## Quick Links

Compile and try out:
- [Build requirements and instructions](docs/build_instructions.md).
- [How to run on qemu](docs/run_on_qemu.md).
- [Overview of the directories](docs/overview_directories.md).
- The [kernel](docs/kernel/kernel.md).
- The [user space](docs/userspace/userspace.md).

How it looks like (text mode via UART only) running on [qemu](docs/run_on_qemu.md):
```
init device ns16550a... OK (1,0)

VIMIX OS 64 bit (bare metal) kernel version 2c539f9 is booting
95KB of Kernel code
8KB of read only data
1KB of data
76KB of bss / uninitialized data

init memory management...
    RAM S: 0x80000000
 KERNEL S: 0x80000000
 KERNEL E: 0x8003d000
    DTB S: 0x83e00000
    DTB E: 0x83e01c20
    RAM E: 0x84000000
init process and syscall support...
init filesystem...
init remaining devices...
init device virtio,mmio... OK (2,0)
init device google,goldfish-rtc... OK (6,0)
init device syscon... OK (7,0)
init device riscv,plic0... OK (8,0)
init device riscv,clint0... OK (9,0)
init device /dev/null... OK (3,0)
init device /dev/zero... OK (4,0)
fs root device: virtio,mmio (2,0)
init userspace...
Memory used: 472kb - 64812kb free
hart 3 starting 
hart 2 starting 
hart 1 starting 
hart 0 starting (init hart)
init: starting /usr/bin/sh
$ ls
drwxr-xr-x      48 B usr
.rwxr-xr-x    3078 B README.md
drwxr-xr-x      80 B dev
drwxr-xr-x      32 B home
$ cat README.md | grep RISC | wc
3 66 496 
$ 
```


## Changes from xv6

- Added documentation in `docs`.
- Cleanups: Reorganized code, separate headers, renamed many functions and variables, using stdint types, general refactoring, reduced number of GOTOs, ...
- Support 32-bit RISC V (in addition to 64-bit), both "bare metal" and running in a [SBI](docs/riscv/SBI.md) environment. Inspired by a 32-bit xv6 port by Michael Schröder (https://github.com/x653/xv6-riscv-fpga/tree/main/xv6-riscv).
- The [user space](docs/userspace/userspace.md) tries to mimics a real UNIX. Some apps can get compiled unchanged for Linux too.
- Changed [memory map](docs/kernel/mm/memory_map_process.md); app stacks grow dynamically.
- Added applications:
	- [stat](docs/userspace/bin/stat.md), [shutdown](docs/userspace/bin/shutdown.md), [mknod](docs/userspace/bin/mknod.md), [date](docs/userspace/bin/date.md), [sleep](docs/userspace/bin/sleep.md), [rmdir](docs/userspace/bin/rmdir.md), [cp](docs/userspace/bin/cp.md), [mount](docs/userspace/bin/mount.md), [umount](docs/userspace/bin/umount.md)
- Added syscalls:
	- [get_dirent](docs/kernel/syscalls/get_dirent.md)
	- [reboot](docs/kernel/syscalls/reboot.md)
	- [get_time](docs/kernel/syscalls/get_time.md)
	- [lseek](docs/kernel/syscalls/lseek.md)
	- split unlink() into [unlink](docs/kernel/syscalls/unlink.md) and [rmdir](docs/kernel/syscalls/rmdir.md)
	- [mount](docs/kernel/syscalls/mount.md) and [umount](docs/kernel/syscalls/umount.md)
- Support multiple [devices](docs/kernel/devices/devices.md), not just two hard coded ones.
- Added devices:
	- [/dev/null](docs/userspace/dev/null.md), [/dev/zero](docs/userspace/dev/zero.md)
	- [ramdisk](docs/kernel/devices/ramdisk.md)
- [xv6 file system](docs/kernel/file_system/xv6fs.md) was changed to differentiate between character and block devices.
- Parse the [device tree](docs/misc/device_tree.md) at boot.