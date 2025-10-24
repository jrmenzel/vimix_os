# User IDs and Group IDs

From the kernels point of view user and group IDs are integers assigned to a [process](../processes/processes.md) that get inherited during [fork](../syscalls/fork.md) and can get changed by certain [syscalls](../syscalls/syscalls.md) (see `struct cred`). It uses these IDs to check access rights of [inodes](../file_system/inode.md) based on the IDs and mode it.

The initial process [init](../../userspace/bin/init.md) gets created with UID/GID 0. ID 0 is special in that it skips some access checks (super user / root).

## Real and Effective IDs

User and group IDs on Linux come in four flavors: 
- real
	- IDs of the actual owner.
- effective
	- Initial value:
		- If binary has `setuid bit` set: eUID = UID from file.
		- If binary has `setgid bit` set: eGID = GID from file.
		- If binary has no special bits set: a copy of the real IDs.
	- Can be changed by some syscalls.
	- IDs used for access checks.
- saved
	- Privilege level the process can return to.
- file system
	- Set by `setfsuid`/`setfsgid` for higher privileged daemons to impersonate a lower privileged user without a full privilege transition. Only affects file related sycalls.
	- VIMIX does not implement this and uses the effective IDs instead.


## Privilege changing Syscalls


All syscalls and wrappers are found in `unistd.h`.

| UID       | GID       | Real ID = 0? | Description                        | Implementation       |
| --------- | --------- | ------------ | ---------------------------------- | -------------------- |
| getuid    | getgid    |              | Get real IDs                       | Wrapper of getresXid |
| geteuid   | getegid   |              | Get effective IDs                  | Wrapper of getresXid |
| getresuid | getresgid |              | Get real, effective and saved IDs. | sys_getresXid        |
| setuid    | setgid    | no           | Set effective IDs if allowed (1).  | sys_setXid           |
| setuid    | setgid    | yes          | Set all IDs.                       | sys_setXid           |
| seteuid   | setegid   | no           | Set effective IDs if allowed (1).  | Wrapper of setresXid |
| seteuid   | setegid   | yes          | Set effective ID.                  | Wrapper of setresXid |
| setresuid | setresgid | no           | Set all IDs if allowed (2).        | sys_setresXid        |
| setresuid | setresgid | yes          | Set all ID.                        | sys_setresXid        |

1. Unprivileged processes can set the effective IDs to the current value, the saved ID or the real ID.
2. Unprivileged processes can set any ID to any value of the currently set (real, effective, saved) IDs.

