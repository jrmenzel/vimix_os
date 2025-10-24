# Syscall setgid

## User Mode

```C
#include <unistd.h.h>

int setgid(gid_t gid);
```

If called from a privileged process, sets the real, effective and saved [group ID](../security/user_group_id.md).

If called from a non-privileged process, sets the effective [group ID](../security/user_group_id.md) if the provided ID is one of the current real, effective or saved group IDs.

## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_setgid()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md)
