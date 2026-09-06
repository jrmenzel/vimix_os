# Syscalls chdir and fchdir

## User Mode

```C
#include <unistd.h>
int32_t chdir(const char *path);

int32_t fchdir(int fd);
```

Change the working directory of the process.
`chdir()` identifies the new directory by path, while `fchdir()` uses an open file descriptor.

## Kernel Mode

Implemented in `sys_file.c` as `sys_chdir()` and `sys_fchdir()`.

## See also

**Overview:** [syscalls](syscalls.md)

**Process Control Syscalls:** [fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
