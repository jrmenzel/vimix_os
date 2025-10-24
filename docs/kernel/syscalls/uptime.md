# Syscall uptime

## User Mode

```C
#include <unistd.h>
int32_t uptime();
```

System uptime in ticks.

## Kernel Mode

Implemented in `sys_system.c` as `sys_uptime()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**System:** [mount](mount.md) | [umount](umount.md) | [uptime](uptime.md)
