# Syscall open

## User Mode

```C
#include <fcntl.h>
int open(const char *name, int32_t flags, ... /*mode_t mode*/)
```


Open a [file](../file_system/file.md), optionally creating it.
`flags` : See `sys/fcntl.h` for flag defines.

**Note:** If flag `O_CREAT` is provided, `mode` must be set (otherwise it gets ignored).


## Kernel Mode

Implemented in `sys_file.c` as `sys_open()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [truncate](truncate.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
