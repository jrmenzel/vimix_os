# Syscalls fstat, stat and lstat

## User Mode

```C
#include <stat.h>

int32_t fstat(FILE_DESCRIPTOR fd, struct stat *buffer);

int32_t stat(const char *path, struct stat *buffer);

int lstat(const char *path, struct stat *buffer);
```

Returns file stats like file type, size and inode number. 

- `fstat` uses an open file descriptor, so the process must be allowed to open the file. `stat` can not be a C wrapper using `fstat` because of this (and potential races).
- `stat` uses a path and can be called for files where the process has no open rights.
- `lstat` reports the meta data of the symlink instead of the meta data of the file the symlink points to.

## User Apps

The app [stat](../../userspace/bin/stat.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_stat()`, `sys_fstat()` and `sys_lstat()`.

## See also

**Overview:** [syscalls](syscalls.md)

**File Information Syscalls:** [stat / fstat / lstat](stat.md) | [readlink](readlink.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [rmdir](rmdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [lseek](lseek.md) | [truncate](truncate.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [rename](rename.md) | [stat](stat.md)
