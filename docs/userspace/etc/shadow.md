# /etc/shadow

On a real UNIX the `/etc/shadow` stores the hashed user passwords and additional information about when it will expire. On VIMIX only the first two columns are used and the password is not hashed.

Each line consists of fields separated by colons:
- user login name
- password
- not used on VIMIX:
	- Minimal days between password changes.
	- Maximal days between password changes.
	- Number of days to warn a user to change the password.
	- Number of days the account will be locked after expiration.
	- Number of days since the account expires completely.
	- Reserved flags.

Additional account information is stored in [/etc/passwd](passwd.md).

## C API

See `shadow.h`.

---
**Up:** [user space](../userspace.md)

**See also:** [group](group.md) | [passwd](passwd.md) | [shadow](shadow.md) 
