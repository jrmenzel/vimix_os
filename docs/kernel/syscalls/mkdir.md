# Syscall mkdir

## User Mode

```C
#include <stat.h>
int32_t mkdir(const char *path, mode_t mode);
```

Create a directory.

## User Apps

The app [mkdir](../../userspace/bin/mkdir.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file.c` as `sys_mkdir()`. 

## See also

**Overview:** [syscalls](syscalls.md)
**File Management Syscalls:**
[mkdir](mkdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
