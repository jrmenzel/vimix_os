# Syscall pipe

## User Mode

```C
#include <unistd.h>
int32_t pipe(int pipe_descriptors[2]);
```

A pipe consists of a buffer in [kernel](../kernel.md) memory and is accessed via two [files](../file_system/file.md): one for writing and one for reading. 

`pipe_descriptors[0]` is the read end, `pipe_descriptors[1]` is the write end of the pipe.

## Kernel Mode

Implemented in `sys_ipc.c` as `sys_pipe()`. A pipe is represented by `struct pipe_t`, which contains the buffer of bytes written into by `pipe_descriptors[1]` and not yet read by `pipe_descriptors[0]`.

## See also

**Overview:** [syscalls](syscalls.md)
