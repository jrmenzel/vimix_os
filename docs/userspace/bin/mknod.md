# mknod- make node

Make device files for character or block [devices](../../kernel/devices/devices.md).

> mknod `name` `type` `major` `minor`

`name`: file name
`type`: `b` for block devices, `c` for character devices
`major`, `minor`: device number

**Returns:**
- 0 on success
**Syscall:** [mknod](../../kernel/syscalls/mknod.md)

---
**Up:** [user space](../userspace.md)

**File Management:** [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mknod](mknod.md) | [rm](rm.md) | [stat](stat.md)
