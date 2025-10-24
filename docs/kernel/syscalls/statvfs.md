# Syscalls statvfs and fstatvfs

## User Mode

```C
#include <statvfs.h>
int statvfs(const char *path, struct statvfs *buf);

int fstatvfs(int fd, struct statvfs *buf);
```

Returns [file system](../file_system/file_system.md) stats. See `struct statvfs` for details.

Both functions do the same, but one takes a path and the other a [file](../file_system/file.md) descriptor. Both exist as syscalls to not force the app to open another file.


## User Apps

The app [statvfs](../../userspace/bin/statvfs.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_filesystem.c` as `sys_statvfs()` and `sys_fstatvfs()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [stat](stat.md)
