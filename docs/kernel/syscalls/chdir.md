# Syscall chdir

## User Mode

```C
#include <unistd.h>
int32_t chdir(const char *path);
```

Change working directory of process.

## Kernel Mode

Implemented in `sys_process.c` as `sys_chdir()`. 

## See also

**Overview:** [syscalls](syscalls.md)
**Process Control Syscalls:**
[fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
