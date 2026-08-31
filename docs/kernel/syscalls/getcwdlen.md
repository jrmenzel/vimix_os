# getcwdlen

## User Mode

```C
#include <unistd.h>
// POSIX-like wrapper
char *getcwd(char buf[], size_t size);
// POSIX-like wrapper
char *get_current_dir_name();

// Actual syscall
ssize_t getcwdlen(char buf[], size_t size);
```

## Kernel Mode

Implemented in `sys_process.c` as `sys_getcwdlen()`. Writes the Current Working Directory of the [process](../processes/processes.md) into `buf` if the `size` is big enough. Returns the length of the string including the trailing zero. Can be used to query the required size by calling `sys_getcwdlen(NULL, 0)`.

## See also

**Overview:** [syscalls](syscalls.md)

**Process Information:** [getpid](getpid.md) | [getcwdlen](getcwdlen.md)
