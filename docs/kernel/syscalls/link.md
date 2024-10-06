# Syscall link

## User Mode

```C
#include <unistd.h>
int32_t link(const char *from, const char *to);
```

Create a new name for a [file](../file_system/file.md) (a hard link).

## User Apps

The app [ln](../../userspace/bin/ln.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file.c` as `sys_link()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
