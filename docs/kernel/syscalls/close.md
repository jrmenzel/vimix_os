# Syscall close

## User Mode

```C
#include <unistd.h>
int32_t close(int fd);
```

Close a [file](../file_system/file.md).

## Kernel Mode

Implemented in `sys_file.c` as `sys_close()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
