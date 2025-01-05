# Block IO Cache

Data on [block devices](../devices/devices.md) are split into blocks of `1024` bytes (`BLOCK_SIZE`) and are indexed by a block ID (0 to device specific maximum).

All block device read / writes go through the block IO buffer which buffers a number of blocks in RAM. See `bio.h` for the API.

The cache is a linked list of buffer entries (`struct buf`) and a global lock (`g_buf_cache`). Each buffer entry contains some meta data (block number, reference count, pointers for the linked list, ...) and the data of this block (`BLOCK_SIZE` bytes).


## Real World

### Blocks and sectors

Disks often have `512` byte block sizes (sectors), the block buffer should be a multiple of the disk block size.

Linux also uses a `1024` byte block buffer.


---
**Overview:** [kernel](kernel.md) | [file_system](file_system.md)

**File System:** [init_filesystem](init_filesystem.md) | [vfs](vfs.md) | [xv6fs](xv6fs/xv6fs.md) | [devfs](devfs.md) | [block_io](block_io.md) | [inode](inode.md) | [file](file.md) | [directory](directory.md)
