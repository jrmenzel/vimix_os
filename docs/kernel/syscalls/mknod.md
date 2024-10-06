# Syscall mknod

## User Mode

```C
#include <stat.h>
int32_t mknod(const char *path, mode_t mode, dev_t dev);
```

Create a special or ordinary file ("make node (in file system)"). Since research UNIX v4.

## User Apps

The app [mknod](../../userspace/bin/mknod.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file.c` as `sys_mknod()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
