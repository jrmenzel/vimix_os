# Syscall umask

## User Mode

```C
#include <sys/stat.h>
mode_t umask(mode_t mask);
```

Change the [processes](../processes/processes.md) `umask`, will return the old mask and never fail. See [file mode](../security/mode.md) regarding how the `umask` impacts the file mode of newly created [files](../file_system/file.md).

## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_umask()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md) | [umask](umask.md)
