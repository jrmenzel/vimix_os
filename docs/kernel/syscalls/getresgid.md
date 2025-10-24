# Syscall getresgid

## User Mode

```C
#include <unistd.h>

int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
```

Query the real, effective and saved [group ID](../security/user_group_id.md).


## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_getresgid()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md)
