# Syscalls chmod and fchmod

## User Mode

```C
#include <sys/stat.h>
int chmod(const char *path, mode_t mode);

int fchmod(int fd, mode_t mode);
```

Change the file mode.

## User Apps

The app [chmod](../../userspace/bin/chmod.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_chmod()` / `sys_fchmod()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md)
