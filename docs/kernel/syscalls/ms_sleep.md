# Syscall sleep

## User Mode

```C
#include <unistd.h>
extern int32_t sleep(int32_t seconds);
extern int32_t ms_sleep(int32_t milliseconds);
```

Lets the process sleep for a given amount of seconds (`sleep`) or milli seconds (`ms_sleep`). The actual time slept can be longer due to larger scheduler intervals ([timer interrupts](../interrupts/timer_interrupt.md) could dictate a time granularity of 10 to 100 milliseconds).

## Kernel Mode

Implemented in `sys_process.c` as `sys_sleep()`. 

## See also

**Overview:** [syscalls](syscalls.md)

**Process Control Syscalls:** [fork](fork.md) | [execv](execv.md) | [exit](exit.md) | [kill](kill.md) | [ms_sleep](ms_sleep.md) | [wait](wait.md) | [chdir](chdir.md) | [sbrk](sbrk.md)
