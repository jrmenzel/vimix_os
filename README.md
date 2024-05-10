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

How it looks like:
```
VIMIX OS 64 bit (bare metal) kernel version 5d6b215 is booting
35KB of Kernel code
4KB of read only data
0KB of data
75KB of bss / uninitialized data

init memory management...
init process and syscall support...
init filesystem...
init userspace...
Memory used: 584kb
hart 0 starting 
hart 2 starting 
hart 3 starting (init hart)
hart 1 starting 
init: starting /usr/bin/sh
$ ls
drwxr-xr-x      48 B usr
.rwxr-xr-x    1183 B README.md
drwxr-xr-x      48 B dev
drwxr-xr-x      32 B home
$ ls /usr/bin
/usr/bin:
.rwxr-xr-x   36984 B init
.rwxr-xr-x   37208 B cat
.rwxr-xr-x   26416 B echo
.rwxr-xr-x   36496 B forktest
.rwxr-xr-x   38816 B grep
.rwxr-xr-x   38680 B kill
.rwxr-xr-x   47992 B ls
.rwxr-xr-x   36368 B ln
.rwxr-xr-x   52184 B sh
.rwxr-xr-x   36440 B rm
.rwxr-xr-x   36504 B mkdir
.rwxr-xr-x   46208 B grind
.rwxr-xr-x   37328 B wc
.rwxr-xr-x   26480 B zombie
.rwxr-xr-x   38288 B stat
.rwxr-xr-x  130264 B usertests
$ cat README.md | grep RISC | wc
2 58 463 
$ 
```


## Changes from xv6

- Added documentation in `docs`.
- Cleanups: Reorganized code, separate headers, renamed many functions and variables, using stdint types, general refactoring, reduced number of GOTOs, ...
- Support 32-bit RISC V (in addition to 64-bit), both "bare metal" and running in a [SBI](docs/riscv/SBI.md) environment. Inspired by a 32-bit xv6 port by Michael Schröder (https://github.com/x653/xv6-riscv-fpga/tree/main/xv6-riscv).
- The [user space](docs/userspace/userspace.md) tries to mimics a real UNIX. Some apps can get compiled unchanged for Linux too.
- Added applications:
	- [stat](docs/userspace/bin/stat.md)
	- [shutdown](docs/userspace/bin/shutdown.md)
	- [mknod](docs/userspace/bin/mknod.md)
	- [date](docs/userspace/bin/date.md)
- Added syscalls:
	- [get_dirent](docs/kernel/syscalls/get_dirent.md)
	- [reboot](docs/kernel/syscalls/reboot.md)
	- [get_time](docs/kernel/syscalls/get_time.md)
- Support multiple [devices](docs/kernel/devices/devices.md), not just two hard coded ones.
- Added devices:
	- [/dev/null](docs/userspace/dev/null.md), [/dev/zero](docs/userspace/dev/zero.md)
	- [ramdisk](docs/kernel/devices/ramdisk.md)
- [xv6 file system](docs/kernel/file_system/xv6fs.md) was changed to differentiate between character and block devices.