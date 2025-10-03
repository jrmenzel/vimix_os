# VimixFS Log

## Motivation

[Syscalls](../../syscalls/syscalls.md) might require, that multiple blocks of the [file_system](file_system.md) need to be modified. For example the creation of a [file](file.md) requires a new [inode](inode.md) and an entry into the parent [directory](directory.md). If these are stored in different blocks of the file system (which they normally are), a system crash between these two (or more) writes could leave the file system in an invalid state.

The solution is a log of all changes to file system blocks that belong to one system call. Once the call completed all blocks are written to disk. This is done in two steps: 
1. First into a reserved area on the disk, the header is the last block to be written.
2. Second into the destination block addresses. 
Next the reserved area header is cleared again.

This way a crash can not result in an invalid file system state:
- If the crash happens before the log is written to disk completely (step 1): The log header will still be cleared and no changes have been made to the file system. The file system has the state of just before the system call.
- If the crash happens after the log is written, but before (all) the blocks have been also written to their destination addresses: A file system check can detect that the log header is not cleared and copy all blocks from the log to the destination addresses. All required info is in the log header. This will restore the state of the file system from after the syscall.
- If the crash happens after the header was cleared: All blocks of the syscall have been written, the file system state is valid.


## Block IO

All reads happen via `bio_read()` from the [Block IO Cache](block_io.md). But writes should use `log_write()` instead of `bio_write()`.

All blocks which are uncommitted in the log are also in the buffer cache, so reads will always get the updated version from the cache.


## Log in VimixFS

The log from the motivation was keeping track of all block writes of one system call. If multiple threads want to access the file system in parallel, multiple system calls must be tracked in the log at once (the alternative is a global lock for file system transactions).

System calls that can modify the [Vimix file system](vimixfs.md) must call `log_begin_fs_transaction()` / `log_end_fs_transaction()` before/after all function calls that might modify the file system. These calls keep track of the number of active system calls to be tracked (`outstanding`). If there are already enough active calls to potentially fill up the log space, the thread waits in `log_begin_fs_transaction()` until the log is committed. It also waits if a commit is in process. `log_end_fs_transaction()` will also trigger a commit in case it was the last active syscall.

A commit is called when the log filled up or if no system calls are tracked anymore. It will write out all blocks from the log. While the log contains blocks from potentially multiple system calls, the process of writing the log first to a log area, then to the destination is the same as in the motivation above.

If multiple VimixFS file systems are [mounted](../../syscalls/mount.md) at the same time, each will have its own log (it is tied to the log area on the block device!).


### Limitations

A [write](../../syscalls/write.md) system call can be split into multiple log transactions if the amount of data to write to the file exceeds the log space (see `MAX_OP_BLOCKS`).


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vimixfs](vimixfs.md) | [vimixfs_log](vimixfs_log.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
