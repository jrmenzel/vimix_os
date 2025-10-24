# Syscall getpid

## User Mode

```C
#include <unistd.h>
pid_t getpid();
```

Get process `pid`.

## Kernel Mode

Implemented in `sys_process.c` as `sys_getpid()`.

## See also

**Overview:** [syscalls](syscalls.md)
