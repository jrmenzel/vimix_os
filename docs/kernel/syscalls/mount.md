# Syscall mount

## User Mode

```C
#include <sys/mount.h>
int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data);
```

Mounts a [file system](../file_system/file_system.md). `mountflags` and `data` are currently ignored (present for compatibility).


## User Apps

The app [mount](../../userspace/bin/mount.md) exposes this syscall.


## Kernel Mode

Implemented in `sys_system.c` as `sys_mount()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**System:** [mount](mount.md) | [umount](umount.md) | [uptime](uptime.md)
