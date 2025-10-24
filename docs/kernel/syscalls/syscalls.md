# System Calls

See also: [calling_syscall](calling_syscall.md).


**Process Control**
- [fork](fork.md) - Fork process.
- [execv](execv.md) - Execute another binary.
- [exit](exit.md) - Exit process.
- [kill](kill.md) - Send signal to a process.
- [ms_sleep](ms_sleep.md) - Sleep for some time.
- [wait](wait.md) - Wait for child process to exit.
- [chdir](chdir.md) - Change the current directory (see proc cwd).
- [sbrk](sbrk.md) - Allocate/free process heap.

**Process Information**
- [getpid](getpid.md) Get PID of the current [process](../processes/processes.md).

**File Management**
- [mkdir](mkdir.md) - create [directory](file_system/directory.md)
- [get_dirent](get_dirent.md) - get directory entries
- [mknod](mknod.md) - make nodes in file system
- [open](open.md) - (optionally create and) open file
- [close](close.md) - close file
- [read](read.md) - read from file
- [write](write.md) - write to file
- [lseek](lseek.md) - get / set file read position
- [truncate](truncate.md) - Change file size.
- [dup](dup.md) - duplicate file handle
- [link](link.md) - create hard link
- [unlink](unlink.md) - remove a hard link, also used to delete files
- [rmdir](rmdir.md) - remove empty directories

**File Meta Data Management**
- [chown](chown.md) - Change file ownership.
- [chmod](chmod.md) - Change file mode.
- [getresgid](getresgid.md) - Get [process](../processes/processes.md) [group IDs](../security/user_group_id.md).
- [getresuid](getresuid.md) - Get [process](../processes/processes.md) [user IDs](../security/user_group_id.md).
- [setuid](setuid.md) - Set [process](../processes/processes.md) effective [user ID](../security/user_group_id.md).
- [setgid](setgid.md) - Set [process](../processes/processes.md) effective [group IDs](../security/user_group_id.md).
- [setresgid](setresgid.md) - Set any [process](../processes/processes.md) [user ID](../security/user_group_id.md).
- [setresuid](setresuid.md) - Set any [process](../processes/processes.md) [user ID](../security/user_group_id.md).

**File Information**
- [stat / fstat](stat.md) - get file metadata

**System**
- [reboot](reboot.md) - halt or reboot the system
- [mount](mount.md) - mount [file_system](../file_system/file_system.md)
- [umount](umount.md) - unmount file system
- [uptime](uptime.md) - returns how log the system is running
- [statvfs](../../userspace/bin/statvfs.md) - get file system statistics

**Communication**
- [pipe](pipe.md) - Pipe between two processes.

**Misc**
- [clock_gettime](clock_gettime.md) - Get the current UNIX time.


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)

**Subsystems:** [interrupts](../interrupts/interrupts.md) | [devices](../devices.md) | [file_system](file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls.md)
