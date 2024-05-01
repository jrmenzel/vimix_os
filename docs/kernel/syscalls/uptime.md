# Syscall uptime

## User Mode

```C
#include <uinstd.h>
int32_t uptime();
```

System uptime in ticks.

## Kernel Mode

Implemented in `sys_system.c` as `sys_uptime()`. 

## See also

**Overview:** [syscalls](syscalls.md)
