# /usr/bin/init

## Process 1: init

Implemented in `usr/bin/init.c`. First user space program to run.

First it opens the `console` character device as [/dev/console](../dev/console.md) so that [file descriptor](../../kernel/file_system/file.md) `0`,`1` and `2` are set to `console`. These file descriptors are defined to map to `stdin`, `stdout` and `stderr` in C (see [stdio](../../misc/stdio.md)).

Then it forks, the parent will [wait](../../kernel/syscalls/wait.md) on the child to close and fork again if that happens.
The child will [execv](../../kernel/syscalls/execv.md) into the shell.

**init is not expected to return**


## Shell

Init starts a [shell](sh.md) and restarts it in case it closes. The shell uses [fork](../../kernel/syscalls/fork.md) and [execv](../../kernel/syscalls/execv.md) to start user applications. For details see [sh](sh.md) and from the [kernel](../../kernel/kernel.md) perspective: [life_cycle_user_application](../../kernel/overview/life_cycle_user_application.md).

**Note:** If the file `/tests/autoexec.sh` exists, the shell will be started with this file as a parameter. This script will be executed in a loop so it should call [shutdown](shutdown.md) for automated testing.


---
**Up:** [user space](../userspace.md)
