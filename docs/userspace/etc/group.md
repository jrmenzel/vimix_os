# /etc/group

Stores all groups with their members.

Each line consists of fields separated by colons:
- Group name.
- Historically the password, must be "x" to indicate `etc/passwd` is used for the password.
- Group ID.
- Members in a comma separated list.

## C API

See `grp.h`.

---
**Up:** [user space](../userspace.md)

**See also:** [group](group.md) | [passwd](passwd.md) | [shadow](shadow.md) 
