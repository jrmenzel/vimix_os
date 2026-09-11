# Syscall symlink

## User Mode

```C
#include <unistd.h>
int symlink(const char *target, const char *link_path);
```

Create a symbolic link at `link_path` containing `target`. The target path is stored without verifying its existence. It can be an absolute path or a relative one. Relative targets are resolved relative to the directory containing the symbolic link when the link is later followed.

The new link does not apply the `umask` of the calling [process](../processes/processes.md).

## User Apps

The app [ln](../../userspace/bin/ln.md) exposes this syscall with `ln -s`.

## Kernel Mode

Implemented in `sys_link.c` as `sys_symlink()`.

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [truncate](truncate.md) | [dup](dup.md) | [link](link.md) | [symlink](symlink.md) | [unlink](unlink.md) | [rename](rename.md)
