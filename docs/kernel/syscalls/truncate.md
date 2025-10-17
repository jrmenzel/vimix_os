# Syscall truncate

## User Mode

```C
#include <unistd.h>
int32_t truncate(const char *path, off_t length);

int32_t ftruncate(int fd, off_t length);
```

Truncates a [file](../file_system/file.md) to the given length. If the file was smaller, it gets filled with zeroes, if it was larger, the extra content gets discarded.



## Kernel Mode

Implemented in `sys_file.c` as `sys_truncate()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [truncate](truncate.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
