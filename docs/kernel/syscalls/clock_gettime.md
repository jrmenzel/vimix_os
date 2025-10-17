# Syscall clock_gettime

## User Mode

Syscall:
```C
ssize_t clock_gettime(clockid_t clockid, struct timespec *tp);
```

Preferred user API:
```C
#include <time.h>

// gets seconds and nanoseconds
int clock_gettime(clockid_t clockid, struct timespec *tp);

// only gets seconds
time_t time(time_t *tloc);
```

`timespec` stores seconds and nanoseconds with a clock depending start time and accuracy.

- `CLOCK_REALTIME` Seconds start on 1970-01-01.
- `CLOCK_MONOTONIC` Unknown start but monotonic (which `CLOCK_REALTIME` isn't if the time gets set manually at some point).
- **Note:** VIMIX does not differentiate between the two and tries to get a time from a real-time clock or even guesses by the number of passed timer interrupts.

`time_t` is **always** 64-bit (even on 32-bit systems) and stores the seconds since 1970-01-01. If it would be 32-bit, it would overflow on 2038-01-19.


## Kernel Mode

Implemented in `sys_system.c` as `sys_clock_gettime()`. Calls `rtc_get_time()` which queries the current time from a real-time clock driver. If no RTC is present, the time since boot gets returned. So it's guaranteed that this value increases (roughly) at a speed of the "wall clock", even if the date is wrong.


## See also

**Overview:** [syscalls](syscalls.md)

