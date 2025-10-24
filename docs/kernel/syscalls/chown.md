# Syscalls chown and fchown

## User Mode

```C
#include <unistd.h>
int chown(const char *path, uid_t owner, gid_t group);

int fchown(int fd, uid_t owner, gid_t group);
```

Change the files [owner and group](../security/user_group_id.md).

## User Apps

The app [chown](../../userspace/bin/chown.md) exposes this syscall.

## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_chown()` / `sys_fchown()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md)
