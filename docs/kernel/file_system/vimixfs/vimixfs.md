# VimixFS

VimixFS is a modified version of the xv6 [file system](file_system.md), which was designed after the original UNIX version 6 file system. 


## Disk Layout

The disk is organized in blocks of `1024` bytes.

|                  | Boot | Super Block | Log Area | Inode Area    | BMap Area        | Data Blocks |
| ---------------- | ---- | ----------- | -------- | ------------- | ---------------- | ----------- |
| Blocks (example) | 1    | 1           | e.g. 32  | ~2% of blocks | 1/8192 of blocks | most blocks |


**Boot:**
The first block is reserved for the boot loader for historic reasons. This block is currently not used.

**Super Block:**
This block contains the struct `vimixfs_superblock` which describes the [file system](file_system.md), including the position and size of the following areas. Everything in here stays static after the file system got created.

**Log Area:**
The first block contains the log header struct `logheader`. It contains a count of blocks to be committed and the destination block addresses. The following blocks are used to store the data for blocks to be committed from the log. The number of these blocks is stored in the super block. 

See [vimixfs_log](vimixfs_log.md).

**Inode Area:**
The [inodes](inode.md) are stored sequentially on disk in an array as `struct vimixfs_dinode` (disk inode). The inode number is the index into this array. The amount of blocks reserved for this inode array defines the maximum of supported inodes and thus files and directories. 

Inode 1 is by definition the root directory of the file system.

[Directories](directory.md) are stored as files containing an array of `struct vimixfs_dirent`.
[Files](file.md) are stored in blocks in the data blocks area. The disk inode contains an array of block addresses for the files content. An additional `indirect block` can be allocated for block addresses of larger files. For even larger files a block with addresses to another level of `indirect blocks` can be allocated. This structure limits the maximum file size.

**BMap Area:**
Stores one bit per block as a use/free flag. The size of this area is defined by the size of the file system. Only blocks from the data blocks can be free, all blocks containing file system meta data (including this bitmap) are indicated as used.

**Data Blocks:**
Unstructured data blocks with the contents of the files and directories as well as `indirect blocks` for large files. The size of this area defines the usable disk size of the file system.


## Limits

**Max files:**
File and directory count are limited by the inode count. This is defined at file system creation in `mkfs` (see `ninodes` in `vimixfs_superblock`). As [directories](directory.md) references inodes with a 16-bit unsigned integer, the maximal number of inodes in VimixFS is 65,535 (0 is a reserved value). Per default `mkfs` will use about 2% of the disk area for inodes (`1/64`).

**Max file size:**
Currently a disk inode can reference `20` data blocks directly. One additional block can be used as a second array of `1024/sizeof(uint32_t)` = `256` block addresses. Another block can be used to index additional indirect blocks for `256*256` additional data blocks.
The maximal file size supported is thus `(20 + 256 + 256*256) blocks` = `65812 kb`.

**Max files per directory;**
A `struct vimixfs_dirent` is `64` bytes long, thus 16 directory entries can be stored in one block. A file can be up to `65812` blocks large. This limits the number of directory entries per directory to `1,052,992` entries. Each dir contains `.` and `..`, so the usable number of files and/or sub directories will be slightly lower.

**Max file system size:**
Blocks are indexed by 32-bit unsigned integers. Thus file systems of up to 4 GB should be possible. 

**Max file name:**
Just 60 chars, limited by `VIMIXFS_NAME_MAX` and indirectly by the size of one `struct vimixfs_dirent`.


## Changes compared to xv6

- Refactored and renamed some defines, moved code to separate generic and VimixFS specific code.
- Moved out non-file system specific function calls (e.g. file system tree traversal calls) from [vimixfs_log](vimixfs_log.md) specific calls. This allows a [virtual file system](../vfs.md) abstraction and [mounting](../../syscalls/mount.md). It also changes the order of lock acquire/releases compared to xv6.
- Split device disk inodes into char and block devices to correctly store different types of [devices](../../devices/devices.md).
- Increased max file name from 14 to 60.
- Changed meta data stored per inode to add missing fields.
- Added a double indirect mapping of blocks for files above 256 kb.


## Related

Video about the original xv6 file system: https://www.youtube.com/watch?v=o9sYiWj1F28&ab_channel=hhp3

---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [vimixfs](vimixfs.md) | [devfs](devfs.md) | [sysfs](sysfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
