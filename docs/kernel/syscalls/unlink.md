# Syscall unlink

## User Mode

```C
#include <unistd.h>
int32_t unlink(const char *pathname);
```

Remove a link from a file, the last `unlink()` will also delete the [file](../file_system/file.md) if there is no reference to that [inode](../file_system/inode.md) anymore. This means that if some process still has the file open, it will continue to exist until the process closes it (then it will be effectively deleted).


## User Apps

The app [rm](../../userspace/bin/rm.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file.c` as `sys_unlink()`. 

## See also

Syscall [rmdir](rmdir.md) to delete [directories](../file_system/directory.md).

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [truncate](truncate.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
