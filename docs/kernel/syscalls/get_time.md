# Syscall get_time

## User Mode

Syscall:
```C
ssize_t get_time(time_t *tloc);
```

Preferred user API:
```C
#include <time.h>
time_t time(time_t *tloc);
```

`time_t` is **always** 64-bit (even on 32-bit systems) and stores the seconds since 1970-01-01. If it would be 32-bit, it would overflow on 2038-01-19.


## Kernel Mode

Implemented in `sys_system.c` as `sys_get_time()`. Calls `rtc_get_time()` which queries the current time from a real-time clock driver. If no RTC is present, the time since boot gets returned. So it's guaranteed that this value increases at a speed of the "wall clock", even if the date is wrong.


## See also

**Overview:** [syscalls](syscalls.md)

