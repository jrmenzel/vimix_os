# Syscall execv

## User Mode

```C
#include <unistd.h>
int32_t execv(const char *pathname, char *argv[]);
```

Execute program at `pathname` replacing the currently running process. `argv` is a NULL terminated array of program parameters, `argv[0]` contains the programs name by convention.

**Note:**
The open file descriptors stay the same.

**Returns:**
- `-1` on error (e.g. file to execute not found)
- **won't return** on success

## Kernel Mode

Implemented in `sys_process.c` as `sys_execv()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**Process Control Syscalls:** [fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
