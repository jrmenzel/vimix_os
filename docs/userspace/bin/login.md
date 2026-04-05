# Login

Login asks for a user name and password and checks the credentials against the content of [/etc/passwd](../etc/passwd.md) and [/etc/shadow](../etc/shadow.md). If the credentials match, the current working directory is set to the users home and the users [user and group IDs](../../kernel/security/user_group_id.md) are set. Then login will [execv](../../kernel/syscalls/execv.md) the users shell ([sh](sh.md)).

It is run from [init](init.md) which will re-start it in case it returns (wrong login data or the user called `exit` in the shell).

Login parses `/etc/login.conf` to enable automatic login and a shell script to automatically run (e.g. for automated testing, see: [getting started](../../development/getting_started.md)).

---
**Up:** [user space](../userspace.md)

**OS applications:** [init](init.md) | [login](login.md)
