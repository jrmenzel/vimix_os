# Syscall rename

## User Mode

```C
#include <unistd.h>
int32_t rename(const char *oldpath, const char *newpath);
```

Atomically changes the name or parent directory of a file or directory. An existing compatible destination is replaced.
Both paths must be on the same mounted file system.

## User Apps

The app [mv](../../userspace/bin/mv.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_link.c` as `sys_rename()`.

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [truncate](truncate.md) | [dup](dup.md) | [link](link.md) | [symlink](symlink.md) | [unlink](unlink.md) | [rename](rename.md)
