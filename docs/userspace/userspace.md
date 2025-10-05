# Userspace


## Init in Userspace

The [kernel](../kernel/kernel.md) starts the [init](bin/init.md) program as the first process (for details see [init_userspace](../kernel/processes/init_userspace.md)).
This normally starts a shell ([sh](bin/sh.md)) for users to start any application.


## Applications

### System Applications

[/usr/bin/init](bin/init.md) - the first process


### User Applications

Stored in `/usr/bin` or `/usr/local/bin`.

**Process Management:**
- [kill](bin/kill.md) - send signals to [processes](../kernel/processes/processes.md)

**File Management:**
- [cp](bin/cp.md) - copy a file
- [ln](bin/ln.md) - create hard links to files
- [ls](bin/ls.md) - list directory content
- [mkdir](bin/mkdir.md) - make new directories
- [mknod](bin/mknod.md) - make device files for character or block [devices](../kernel/devices/devices.md)
- [rm](bin/rm.md) - remove files
- [rmdir](bin/rmdir.md) - remove directories
- [stat](bin/stat.md) - get file status
- [statvfs](bin/statvfs.md) - get [file system](../kernel/file_system/file_system.md) statistics

**Misc:**
- [cat](bin/cat.md) - concatenate input files and print them
- [xxd](bin/xxd.md) - hex dump
- [echo](bin/echo.md) - echo parameters back
- [grep](bin/grep.md) - find a string in a file
- [wc](bin/wc.md) - count words and characters in a text file
- [date](bin/date.md) - Prints date and time
- [sleep](bin/sleep.md) - Pauses execution for N seconds
- [time](bin/time.md) - Print execution time of an application
- [dhrystone](local/bin/dhrystone.md) - A benchmark

**System:**
- [fsinfo](bin/fsinfo.md) - info on mounted [file systems](../kernel/file_system/file_system.md)
- [sh](bin/sh.md) - the shell
- [which](bin/which.md) - finds programs in the search path
- [mount](bin/mount.md) - mounts a [file system](../kernel/file_system/file_system.md)
- [umount](bin/umount.md) - unmounts a [file system](../kernel/file_system/file_system.md)
- [shutdown](bin/shutdown.md) - Halts or shuts down the OS
- [meminfo](bin/meminfo.md) - prints info on memory usage

**Games:**
- [fortune](bin/fortune.md) - classic UNIX fortune cookie
- [wumpus](bin/wumpus.md) - UNIX 7 version of Hunt the Wumpus

**Tests:**
- [usertests](tests/usertests.md) - various automated tests
- [forktest](tests/forktest.md) - tests [fork](../kernel/syscalls/fork.md) syscall
- [grind](tests/grind.md) - tests random [syscalls](../kernel/syscalls/syscalls.md) forever (wont return)
- [zombie](tests/zombie.md) - tests if a zombie process is correctly handled
- [skill](tests/skill.md) - tests a stack overflow


### Linux support

Some user space apps compile also on Linux. See [build_instructions](../build_instructions.md) for details.


## File System

**/etc**
- Config files

**/dev:**
- All devices, automatically added by the [devfs](../kernel/file_system/devfs/devfs.md)
- [/dev/console](dev/console.md)
- [/dev/null](dev/null.md)
- [/dev/random](dev/random.md)
- [/dev/zero](dev/zero.md)
- [/dev/temp](dev/temp.md) (optional)

**/home:**
- [mount point](bin/mount.md) for an optional second file system

**/tests:**
- Shell scripts for automated testing

---
**Up:** [README](../../README.md)

[build_instructions](../build_instructions.md) | [overview_directories](../overview_directories.md) | [kernel](../kernel/kernel.md) | [user space](userspace.md)
