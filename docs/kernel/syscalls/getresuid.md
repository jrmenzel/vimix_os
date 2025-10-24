# Syscall 

## User Mode

```C
#include <unistd.h>

int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
```

Query the real, effective and saved [user ID](../security/user_group_id.md).


## Kernel Mode

Implemented in `sys_file_meta.c` as `sys_getresuid()`. 


## See also

**Overview:** [syscalls](syscalls.md)

**File Meta Data Syscalls:** [chmod](chmod.md) | [chown](chown.md) | [getresgid](getresgid.md) | [getresuid](getresuid.md) | [setuid](setuid.md) | [setgid](setgid.md) | [setresgid](setresgid.md) | [setresuid](setresuid.md)
