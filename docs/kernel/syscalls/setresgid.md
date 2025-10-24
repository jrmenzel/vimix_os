# Syscall setresgid

## User Mode

```C
#include <unistd.h>

int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
```

Sets the real, effective and saved [group IDs](../security/user_group_id.md) to the provided ID unless they are -1.

If called from a non-privileged process, the provided IDs must be one of the current real, effective or saved IDs.


## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_setresgid()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md)
