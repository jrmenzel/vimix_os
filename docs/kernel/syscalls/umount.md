# Syscall umount

## User Mode

```C
#include <sys/mount.h>
int umount(const char *target);
```

Unmounts a [file_system](../file_system/file_system.md).


## User Apps

The app [umount](../../userspace/bin/umount.md) exposes this syscall.


## Kernel Mode

Implemented in `sys_system.c` as `sys_umount()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**System:** [mount](mount.md) | [umount](umount.md) | [uptime](uptime.md)
