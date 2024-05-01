# Syscall wait

## User Mode

```C
#include <wait.h>
pid_t wait(int *wstatus);
```

Waits on [processe](../processes/processes.md) to exit. Parent processes use this to wait on a [forked](fork.md) child to exit. Convert the `wstatus` into the return code with the `WEXITSTATUS` macro.

## Kernel Mode

Implemented in `sys_process.c` as `sys_wait()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**Process Control Syscalls:** [fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
