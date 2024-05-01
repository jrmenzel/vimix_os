# Syscall read

## User Mode

```C
#include <unistd.h>
ssize_t read(int fd, void *buffer, size_t n);
```

Read `n` chars from a [file](../file_system/file.md) to `buffer`.

## Kernel Mode

Implemented in `sys_file.c` as `sys_()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
