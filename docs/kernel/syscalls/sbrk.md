# Syscall sbrk

## User Mode

```C
#include <unistd.h>
extern void *sbrk(intptr_t increment);
```

Change process heap memory size.

## Kernel Mode

Implemented in `sys_process.c` as `sys_sbrk()`. 

## See also

**Overview:** [syscalls](syscalls.md)
**Process Control Syscalls:**
[fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
