# Syscall dup

## User Mode

```C
#include <unistd.h>
int dup(int fd);
```

Duplicate open [file descriptor](../file_system/file.md).

## Kernel Mode

Implemented in `sys_file.c` as `sys_dup()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
