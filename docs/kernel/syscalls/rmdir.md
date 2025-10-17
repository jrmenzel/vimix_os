# Syscall rmdir

## User Mode

```C
#include <unistd.h>
int32_t rmdir(const char *pathname);
```

Will delete the [directory](../file_system/directory.md) if it is empty.

## User Apps

The app [rmdir](../../userspace/bin/rmdir.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file.c` as `sys_rmdir()`. 

## See also

Syscall [unlink](unlink.md) to delete [files](../file_system/file.md).

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [truncate](truncate.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
