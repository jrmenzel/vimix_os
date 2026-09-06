# Syscall sync

## User Mode

```C
#include <unistd.h>
void sync(void);
```

Syncs all [mounted](mount.md) [file systems](../file_system/file_system.md). By default, it flushes the underlying block devices, but file systems can provide a specific sync function.

## User Apps

The app [sync](../../userspace/bin/sync.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_filesystem.c` as `sys_sync()`.

## See also

**Overview:** [syscalls](syscalls.md)

**System:** [mount](mount.md) | [umount](umount.md) | [sync](sync.md) | [uptime](uptime.md)
