# chown -Change File Ownership

Changes the [file](../../kernel/file_system/file.md) [owner (and optionally group)](../../kernel/security/user_group_id.md). To only change the file group use [[chgrp]].

> chown `user name`:`group name` `file`
> chown `user name` `file`


**Returns:**
- 0 on success
**Syscall:** [chown](chown.md)

---
**Up:** [user space](../userspace.md)

**File Meta Data:** [chgrp](chgrp.md) | [chmod](chmod.md) | [chown](chown.md)
