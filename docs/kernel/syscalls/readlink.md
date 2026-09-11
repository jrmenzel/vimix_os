# Syscall readlink

## User Mode

```C
#include <unistd.h>
ssize_t readlink(const char *path, char *buffer, size_t buffer_size);
```

Copy the target path stored in the symbolic link at `path` into `buffer`.
The final path component must be a symbolic link. No terminating null byte is appended. The return value is the number of bytes copied.

## User Apps

The app [ls](../../userspace/bin/ls.md) uses this syscall to print symbolic links.

## Kernel Mode

Implemented in `sys_link.c` as `sys_readlink()`.

## See also

**Overview:** [syscalls](syscalls.md)

**File Information Syscalls:** [stat / fstat / lstat](stat.md) | [readlink](readlink.md)
