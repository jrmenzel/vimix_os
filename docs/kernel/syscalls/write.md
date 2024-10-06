# Syscall write

## User Mode

```C
#include <unistd.h>
ssize_t write(int fd, const void *buffer, size_t n)
```

Write to a [file](../file_system/file.md).

## Kernel Mode

Implemented in `sys_file.c` as `sys_write()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
