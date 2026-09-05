# /dev/console

Console / UART IO. Opened by [init](../bin/init.md) to be the [C standard IO](../../misc/stdio.md) for all apps.

There can be multiple consoles, one per detected TTY device.
The first console `/dev/console0` is used for kernel messages.

---
**Up:** [user space](../userspace.md)

**See also:** [console](console.md) | [null](null.md) | [random](random.md) | [temp](temp.md) | [zero](zero.md)
