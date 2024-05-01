# File System

See also: [init_filesystem](init_filesystem.md).

## Block IO

Data on block devices (see [devices](../devices/devices.md)) are split into blocks of `1024` bytes and are indexed by a block ID (0 to device specific maximum).
The first block, called `superblock`, will contain the information of how many blocks there are.

All block device read / writes go through the block IO buffer which buffers a number of blocks in RAM. See `bio.h` for the API.


## Supported File Systems

File systems supported:
- mostly original [xv6 filesystem](xv6fs.md) (`sys/fs/xv6fs`)


## Real World

### Blocks and sectors

Disks often have `512` byte block sizes (sectors), the block buffer should be a multiple of the disk block size.

Linux also uses a `1024` byte block buffer.


---
**Overview:** [kernel](kernel.md)
**Boot:**
[boot_process](../overview/boot_process.md) | [init_overview](../overview/init_overview.md)
**Subsystems:**
[interrupts](../interrupts/interrupts.md) | [devices](../devices/devices.md) | [file_system](file_system.md) | [memory_management](../mm/memory_management.md)
[processes](../processes/processes.md) | [scheduling](../processes/scheduling.md) | [syscalls](../syscalls/syscalls.md)
