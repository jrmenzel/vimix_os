# mv - move files

Moves or renames a file or directory. If `TO` is a directory, the call is equivalent to `cp FROM TO/FROM`.

> mv `FROM` `TO`

Within the same [file system](../../kernel/file_system/file_system.md) the [rename](../../kernel/syscalls/rename.md) [syscall](../../kernel/syscalls/syscalls.md) will be called.

Only regular files can be moved between file systems by copying their contents to
the destination and removing the source after the copy completes. A temporary file is used during the copy. This fallback is not atomic. 

**Syscall:** [rename](../../kernel/syscalls/rename.md)

---

**Up:** [user space](../userspace.md)

**File Management:** [cp](cp.md) | [ln](ln.md) | [ls](ls.md) | [mkdir](mkdir.md) | [mv](mv.md) | [mknod](mknod.md) | [rm](rm.md) | [rmdir](rmdir.md) | [stat](stat.md) | [statvfs](statvfs.md)
