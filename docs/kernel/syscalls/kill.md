# Syscall kill

## User Mode

```C
#include <signal.h>
int32_t kill(pid_t pid);
```

Kills the process with given [process](../processes/processes.md) ID `pid`. 

## User Apps

The app [kill](../../userspace/bin/kill.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_process.c` as `sys_kill()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**Process Control Syscalls:** [fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
