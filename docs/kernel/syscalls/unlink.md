# Syscall unlink

## User Mode

```C
#include <unistd.h>
int32_t unlink(const char *pathname);
```

Remove a link from a file, the last `unlink()` will also delete the [file](../file_system/file.md).

## User Apps

The app [rm](../../userspace/bin/rm.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file.c` as `sys_unlink()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
