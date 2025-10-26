# File Mode

The file mode (stored as `mode_t`) encodes access rights and a few special bits. [Userspace](../../userspace/userspace.md) changes these flags with the [chmod](../../userspace/bin/chmod.md) app. [ls](../../userspace/bin/ls.md) prints them.

## Access Rights

There are three blocks of access rights, each encoding read, write and execute right. 

1. User access: If the [effective user ID](user_group_id.md) of the [process](../processes/processes.md) matches the [files](../file_system/file.md) user ID, these flags are evaluated.
2. Group access: If the [effective group ID](user_group_id.md) or any supplemental group ID of the [process](../processes/processes.md) matches the [files](../file_system/file.md) group ID, these flags are evaluated.
3. Other access: If neither the user, nor the group IDs match, use these flags.

Note that only one group of flags is ever evaluated. E.g. if the user ID matches but the access rights do not permit the operation, the group and other flags will not get checked (even if they allow more access).

Access rights are written in octal notation with three places for user, group and other access flags.

| Value | Can Read | Can Write | Can Execute |
| ----- | -------- | --------- | ----------- |
| 0     |          |           |             |
| 1     |          |           | yes         |
| 2     |          | yes       |             |
| 3     |          | yes       | yes         |
| 4     | yes      |           |             |
| 5     | yes      |           | yes         |
| 6     | yes      | yes       |             |
| 7     | yes      | yes       | yes         |


### UID 0 / root

Processes with effective user ID 0 (`root`) will only check the access rights if the file is about to be executes. Read and write is always allowed for root. Enforcing an execute bit to be set even for root is a security feature.

If [file systems](../file_system/file_system.md) could be mounted read only (as on a real UNIX), this would also block root from writing to the files.

## umask

When creating a new [file](../file_system/file.md), the provided `mode`is merged with the `umask` of the [process](../processes/processes.md). The default `umask` is `0` but set by [init](../../userspace/bin/init.md) to `022` (turning a create of `0666` into mode `0644`). [fork](../syscalls/fork.md) inherits the parents `umask`. It can be set via the [syscall](../syscalls/syscalls.md) [umask](../syscalls/umask.md).

The mask only impacts the files access rights and can be between `0`and `0777`.


---
**Overview:** [kernel](../kernel.md)

**Security:** [mode](mode.md) | [user_group_id](user_group_id.md)
