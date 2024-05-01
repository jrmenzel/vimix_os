# /init

## Process 1: init

Implemented in `usr/bin/init.c`. First user space program to run.

First it opens the `console` character device so that [file descriptor](../../kernel/file_system/file.md) `0`,`1` and `2` are set to `console`. These file descriptors are defined to map to `stdin`, `stdout` and `stderr` in C (see [stdio](../../misc/stdio.md)).

Then it forks, the parent will [wait](../../kernel/syscalls/wait.md) on the child to close and fork again if that happens.
The child will [execv](../../kernel/syscalls/execv.md) into the shell.

**Note:** If `DEBUG_AUTOSTART_USERTESTS` is defined at compile time, the init process will start and restart the usertests for quicker testing during development ([build_instructions](../../build_instructions.md)).

**init is not expected to return**


## Shell

Init starts a [shell](sh.md) and restarts it in case it closes. The shell uses [fork](../../kernel/syscalls/fork.md) and [execv](../../kernel/syscalls/execv.md) to start user applications. For details see [sh](sh.md) and from the [kernel](../../kernel/kernel.md) perspective: [life_cycle_user_application](../../kernel/overview/life_cycle_user_application.md).


---
**Up:** [user space](../userspace.md)
