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
init device riscv,plic0... OK (8,0)
init device ns16550a... OK (13,0)

VIMIX OS 64 bit (RISC V) kernel version 36bb03d is booting
Console: ns16550a
Timer source: sstc extension
init memory management...
init process and syscall support...
init filesystem...
init remaining devices...
init device /dev/null... OK (3,0)
init device /dev/zero... OK (4,0)
init device /dev/random... OK (10,0)
init device google,goldfish-rtc... OK (6,0)
init device syscon... OK (7,0)
init device virtio,mmio... OK (1,0)
init device virtio,mmio... OK (1,1)
fs root device: virtio,mmio (1,0)
SBI implementation: OpenSBI (version 65541)
SBI specification: v2.0
SBI extension SRST detected: register SBI reboot/shutdown functions
init userspace...
CPU 0 starting 
CPU 2 starting 
CPU 3 starting 
CPU 1 starting (boot CPU)
forkret() mounting /... OK
forkret() loading /usr/bin/init... OK
init mounting /dev... OK
init mounting /sys... OK
init mounting /home... OK
init starting /usr/bin/sh
$ ls
drwxr-xr-x    1104 B .
drwxr-xr-x    1104 B ..
drwxr-xr-x      64 B usr
.rwxr-xr-x    5311 B README.md
drw-rw----       0 B dev
drwxr-xr-x    1024 B etc
drwxr-xr-x     112 B home
drw-rw----       0 B sys
drwxr-xr-x    7168 B tests
$ cat README.md | grep RISC | wc
4 78 607 
$ fortune
I'd spell creat with an e. - Ken Thompson when asked what he would do differently if he were to redesign UNIX
$ 
```


## Changes from xv6

- Added [documentation](docs/overview_directories.md) in `docs`.
- Cleanups: Reorganized code, separate headers, renamed many functions and variables, using stdint types, general refactoring, reduced number of GOTOs, ...
- Support 32-bit RISC V (in addition to 64-bit), both booting in [M-mode](docs/riscv/M-mode.md) and [S-mode](docs/riscv/S-mode.md) in a [SBI](docs/riscv/SBI.md) environment. Inspired by a 32-bit xv6 port by Michael Schröder (https://github.com/x653/xv6-riscv-fpga/tree/main/xv6-riscv).
- The [user space](docs/userspace/userspace.md) tries to mimics a real UNIX. Some apps can get compiled unchanged for Linux too.
- Changed [memory map](docs/kernel/mm/memory_map_process.md); app stacks grow dynamically.
- Added applications:
	- [stat](docs/userspace/bin/stat.md), [shutdown](docs/userspace/bin/shutdown.md), [mknod](docs/userspace/bin/mknod.md), [date](docs/userspace/bin/date.md), [sleep](docs/userspace/bin/sleep.md), [rmdir](docs/userspace/bin/rmdir.md), [cp](docs/userspace/bin/cp.md), [mount](docs/userspace/bin/mount.md), [umount](docs/userspace/bin/umount.md), [fortune](docs/userspace/bin/fortune.md), [wumpus](docs/userspace/bin/wumpus.md), [time](docs/userspace/bin/time.md), [which](docs/userspace/bin/which.md), [meminfo](docs/userspace/bin/meminfo.md), [fsinfo](docs/userspace/bin/fsinfo.md)
- Added syscalls:
	- [get_dirent](docs/kernel/syscalls/get_dirent.md)
	- [reboot](docs/kernel/syscalls/reboot.md)
	- [get_time](docs/kernel/syscalls/get_time.md)
	- [lseek](docs/kernel/syscalls/lseek.md)
	- split `unlink()` into [unlink](docs/kernel/syscalls/unlink.md) and [rmdir](docs/kernel/syscalls/rmdir.md)
	- [mount](docs/kernel/syscalls/mount.md) and [umount](docs/kernel/syscalls/umount.md)
- Support multiple [devices](docs/kernel/devices/devices.md), not just two hard coded ones.
- Added devices:
	- [/dev/null](docs/userspace/dev/null.md), [/dev/zero](docs/userspace/dev/zero.md), [/dev/random](docs/userspace/dev/random.md)
	- [ramdisk](docs/kernel/devices/ramdisk.md)
	- [jh7110 temperature sensor](docs/userspace/dev/temp.md)
- [xv6 file system](docs/kernel/file_system/xv6fs/xv6fs.md) was changed to differentiate between character and block [devices](docs/kernel/devices/devices.md). It was also moved behind a [virtual file system](docs/kernel/file_system/vfs.md) abstraction.
- Parse the [device tree](docs/misc/device_tree.md) at [boot](docs/kernel/overview/boot_process.md).
- Added [/dev](docs/kernel/file_system/devfs/devfs.md) as a special file system exposing all devices.
- Added [/sys](docs/kernel/file_system/sysfs/sysfs.md) as a special file system exposing various kernel objects.
- Boot in [M-mode](docs/riscv/M-mode.md) now mimics [SBI](docs/riscv/SBI.md) to the [S-mode](docs/riscv/S-mode.md) kernel.
- Added a [buddy allocator](docs/kernel/mm/memory_management.md). `kmalloc()` now supports smaller allocations of one [page](docs/kernel/mm/page.md) via a slab allocator (see [memory_management](docs/kernel/mm/memory_management.md)).
- Added [Inter Processor Interrupts](docs/kernel/interrupts/IPI.md).
