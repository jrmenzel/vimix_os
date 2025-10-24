# /etc/passwd

Historically the `/etc/passwd` stored the users login information including the (hashed) password. The password itself has since moved to [/etc/shadow](shadow.md) to allow stricter access control.

Each line consists of fields separated by colons, some fields can be empty:
- User login name.
- Historically the password, now always expected to be "x" to indicate that the password is in [/etc/shadow](shadow.md).
- [User ID](../../kernel/security/user_group_id.md).
- [Group ID](../../kernel/security/user_group_id.md).
- Full name, often unused.
- Home dir.
- Login shell.

## C API

See `pwd.h`.

---
**Up:** [user space](../userspace.md)

**See also:** [group](group.md) | [passwd](passwd.md) | [shadow](shadow.md) 
