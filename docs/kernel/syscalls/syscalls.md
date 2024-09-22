# System Calls

See also: [calling_syscall](calling_syscall.md).


**Process Control**
- [fork](fork.md)
- [execv](execv.md)
- [exit](exit.md)
- [kill](kill.md)
- [ms_sleep](ms_sleep.md)
- [wait](wait.md)
- [chdir](chdir.md) - the current directory is process state (see proc cwd)
- [sbrk](sbrk.md) - allocate/free process heap

**Process Information**
- [getpid](getpid.md) get PID of the current [process](../processes/processes.md).

**File Management**
- [mkdir](mkdir.md) - create [directory](file_system/directory.md)
- [get_dirent](get_dirent.md) - get directory entries
- [mknod](mknod.md) - make nodes in file system
- [open](open.md) - (optionally create and) open file
- [close](close.md) - close file
- [read](read.md) - read from file
- [write](write.md) - write to file
- [lseek](lseek.md) - get / set file read position
- [dup](dup.md) - duplicate file handle
- [link](link.md) - create hard link
- [unlink](unlink.md) - remove a hard link, also used to delete files

**File Information**
- [fstat](fstat.md) - get file metadata

**System Information**
- [uptime](uptime.md)

**Communication**
- [pipe](pipe.md)

**Misc**
- [reboot](reboot.md)
- [get_time](get_time.md) - UNIX time


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](../devices.md) | [file_system](file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls.md)
