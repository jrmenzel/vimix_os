# Syscall reboot

## User Mode

```C
#include <sys/reboot.h>
extern ssize_t reboot(int32_t cmd);

# cmd:
#define VIMIX_REBOOT_CMD_POWER_OFF 0
#define VIMIX_REBOOT_CMD_RESTART 1
```

Reboots or halts the system.

## User Apps

The app [shutdown](../../userspace/bin/shutdown.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_system.c` as `sys_reboot()`. 
**Warning:** Shuts off hard, ideally the OS would write all outstanding transactions to disk first or kill all processes first, etc.


## See also

**Overview:** [syscalls](syscalls.md)
