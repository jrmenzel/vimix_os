# Syscall fstat

## User Mode

```C
#include <stat.h>
int32_t fstat(FILE_DESCRIPTOR fd, struct stat *buffer);

int32_t stat(const char *path, struct stat *buffer);
```

Returns file stats like file type, size, inode number etc. See `struct stat` for details.

## User Apps

The app [stat](../../userspace/bin/stat.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file.c` as `sys_fstat()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
