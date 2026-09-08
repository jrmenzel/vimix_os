# Syscall utimes

## User Mode

```C
#include <utimes.h>
int utimes(const char *path, const struct timeval times[2]);

// wrapper:
#include <sys/time.h>
int utime(const char *path, const struct utimbuf *times);
```

Change the files modification time. 

## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_utimes()`.

## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md) | [umask](umask.md) | [utimes](utimes.md)
