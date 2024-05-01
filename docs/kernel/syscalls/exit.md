# Syscall exit

## User Mode

```C
#include <uinstd.h>
void exit(int32_t status);
```

Terminates the program with status as the return code.
Closes all open files. 
Reparents child processes to init.
The process remains in the zombie state until its parent calls [wait](wait.md).

## Kernel Mode

Implemented in `sys_process.c` as `sys_exit()`. Function never returns, the thread jumps into the scheduler.

## See also

**Overview:** [syscalls](syscalls.md)

**Process Control Syscalls:** [fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
