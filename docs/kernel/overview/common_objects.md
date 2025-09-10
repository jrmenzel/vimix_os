# Common Objects


## Synchronisation

To synchronize multiple cores functions from the C11 atomics can be used, see `stdatomics.h`. Those functions are wrappers around compiler buildins which often map to a single atomic assembly instruction.

Based on these atomics are the following locks:
- `struct spinlock` only allows one thread to hold it, others will busy wait / spin for their turn. 
	- Thread can not get interrupted while holding or waiting on the lock!
	- Threads can try to lock it if spinning is too expensive and access to the object can get delayed. Use `spin_trylock()` for this.
- `struct sleeplock` also only allows one thread to hold it, but other will sleep to get the lock.
	- Used only for user [processes](../processes/processes.md) (the kernel boot threads which call the [scheduler](../processes/scheduling.md) can not wait!).
- `struct rwspinlock` a read / write spinlock with writer priority:
	- Multiple readers can hold it concurrently
	- A writer will wait till all readers have finished, no new readers can get the lock (they spin and wait for the writer to finish).
	- Only one writer is allowed.

Some global counters like `g_next_pid` can get synced with atomic operations directly without an explicit lock.


## Memory Management

For [memory_management](../mm/memory_management.md) `kmalloc()` and `kfree()` can be used.

For managing lists use the functions around the struct `list_head`.


## Clib

A small subset of the standard C library is available (e.g. string functions from `<string.h>` in `kernel/string.h` ). The implementation is shared with the [userspace](../../userspace/userspace.md) Clib.


## Objects

See [object_orientation](object_orientation.md).


---
**Overview:** [kernel](../kernel.md)

**Related:** [common_objects](common_objects.md) | [object_orientation](object_orientation.md)
