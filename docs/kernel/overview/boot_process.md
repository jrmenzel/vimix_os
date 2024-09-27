# Boot Process - How to get from Assembly to main()

On [RISC V](../../riscv/RISCV.md) the [kernel](../kernel.md) expects to be loaded at address `0x80000000`. 

## On bare-metal

All harts (CPU threads) start there in [M-mode](../../riscv/M-mode.md).

0. A boot loader loads the kernels binary into RAM at address `0x80000000` and starts it. Optionally a boot loader might also load a [device tree file](../../misc/device_tree.md) and inform the kernel by placing the pointer to it into register `a1`.
1. `arch/riscv/entry.S` sets up the stack pointer for all harts in assembly, then jumps to C. It also checks if the PC has more CPUs as are being supported and sends the extra ones into an infinite loop.
2. `arch/riscv/start.c` sets up the remaining RISC V specific bits in [M-mode](../../riscv/M-mode.md), e.g. the Timer Interrupts (see [interrupts](../interrupts/interrupts.md)).
3. `init/main.c` is the start for all harts in [S-mode](../../riscv/S-mode.md). This is where the kernel does the actual init of the subsystems (virtual memory, console output, interrupts etc.)


## In a SBI environment

E.g. booted from OpenSBI (see [SBI](../../riscv/SBI.md)).
One hart starts in [S-mode](../../riscv/S-mode.md).

0. A boot loader loads the kernels binary into RAM at address `0x80000000` and starts it. Boot loader will load a [device tree file](../../misc/device_tree.md) and inform the kernel by placing the pointer to it into register `a1`.
1. `arch/riscv/entry.S` sets up the stack pointer for all harts in assembly, then jumps to C. It also checks if the PC has more CPUs as are being supported and sends the extra ones into an infinite loop.
2. `arch/riscv/start.c` sets up the remaining RISC V specific bits in [S-mode](../../riscv/S-mode.md).
3. `init/main.c` is the start for all harts in [S-mode](../../riscv/S-mode.md). This is where the kernel does the actual init of the subsystems (virtual memory, console output, interrupts etc.). The first hart will init SBI and the Timer Interrupts (see [interrupts](../interrupts/interrupts.md)). If the `hart state management` SBI extension is found, additional harts can be started (they also start in `arch/riscv/entry.S` but don't get the device tree pointer).


## Init

See [init_overview](init_overview.md) for the OS and [user space](../../userspace/userspace.md) [init](../processes/init_userspace.md) process.


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](boot_process.md) | [init_overview](init_overview.md)

**See also:** [life_cycle_cpu](life_cycle_cpu.md) [life_cycle_user_application](life_cycle_user_application.md)
