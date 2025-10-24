# STDIO

C defines that for every program three [files](../kernel/file_system/file.md) are open at program start which act as standard IO.

Defined in `stdio.h`:

```C
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
```

The file descriptors of these files are set by convention to `0`,`1` and `2` in `unistd.h`:

```C
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
```

Inside of the [kernel](../kernel/kernel.md) these descriptors are indices into the per-[process](../kernel/processes/processes.md) table of open files. [init](../userspace/bin/init.md) is responsible for opening the console to act as these three files.

As [open](../kernel/syscalls/open.md) is guaranteed to use the smallest available index in the file table and thus the smallest available file descriptor value, processes can change the stdio files by closing one of these file and opening another one directly afterwards. This is how the [sh](../userspace/bin/sh.md) creates file re-directions before calling [fork](../kernel/syscalls/fork.md) and [execv](../kernel/syscalls/execv.md) to run a program.

---
[kernel](kernel/kernel.md) | [user space](userspace/userspace.md)
