# Syscall get_dirent

## User Mode

```C
/// @brief Syscall
size_t get_dirent(int fd, struct dirent *dirp, ssize_t seek_pos);
```

Returns a [directory](../file_system/directory.md) entry for dir `fd` at `seek_pos`. Apps should not call `get_dirent()` directly, but the higher level directory API from `dirent.h`:

```C
#include <dirent.h>

/// @brief Opens a directory stream based on a dir name.
DIR *opendir(const char *name);

/// @brief Opens a directory stream based on a file descriptor.
DIR *fdopendir(int fd);

/// @brief Iterates over all entries of a directory.
struct dirent *readdir(DIR *dirp);

/// @brief Rewind directory stream to beginning of dir.
void rewinddir(DIR *dirp);

/// @brief get current location in directory stream.
long telldir(DIR *dirp);

/// @brief set next dir entry to return from readdir().
void seekdir(DIR *dirp, long loc);

/// @brief Closes a directory stream.
int closedir(DIR *dirp);
```

There is no standard for this syscall, but the wrappers in `dirent.h` are part of the POSIX standard. They are implemented in the stdc lib in `dirent_impl.c`.


## User Apps

The app [ls](../../userspace/bin/ls.md) uses this to list the directory contents.


## Kernel Mode

Implemented in `sys_file.c` as `sys_get_dirent()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Management Syscalls:** [mkdir](mkdir.md) | [get_dirent](get_dirent.md) | [mknod](mknod.md) | [open](open.md) | [close](close.md) | [read](read.md) | [write](write.md) | [dup](dup.md) | [link](link.md) | [unlink](unlink.md) | [fstat](fstat.md)
