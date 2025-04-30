# Boot Process - How to get from Assembly to main()

On [RISC V](../../riscv/RISCV.md) the [kernel](../kernel.md) expects to be loaded at address `0x80000000` on bare metal and address `0x80200000` when running under [SBI](../../riscv/SBI.md). This is defined in the kernels linker script.


## On bare-metal

All harts (CPU threads) start there in [M-mode](../../riscv/M-mode.md).

0. A boot loader loads the kernels binary into RAM at address `0x80000000` and starts it. Optionally a boot loader might also load a [device tree file](../../misc/device_tree.md) and inform the kernel by placing the pointer to it into register `a1`. VIMIX is expecting a device tree (qemu provides one).
1. `arch/riscv/asm/entry.S` calls `_entry_m_mode` from `m_mode.S`.
2. `arch/riscv/m_mode.S` sets up a stack per CPU, calls `m_mode_start()` and picks a boot hart via a lottery. The non-boot harts will wait for an interrupt, which gets triggered when the boot hart starts these harts vi [SBI](../../riscv/SBI.md) later.
3. `arch/riscv/m_mode.c` sets up the remaining RISC V specific bits in [M-mode](../../riscv/M-mode.md) and registers an interrupt handler for the timer (not started yet), illegale instructions (to emulate time/timeh csrs) and [SBI](../../riscv/SBI.md) ecalls.
4. After this setup only the boot hart jumps to `_entry_s_mode` in [S-Mode](../../riscv/S-mode.md). The remaining boot is identical to booting in a real [SBI](../../riscv/SBI.md) environment (except for the linker address).


## In a SBI environment

E.g. booted from OpenSBI (see [SBI](../../riscv/SBI.md)).
One hart starts in [S-mode](../../riscv/S-mode.md).

0. A boot loader loads the kernels binary into RAM at address `0x80200000` and starts it. Boot loader will load a [device tree file](../../misc/device_tree.md) and inform the kernel by placing the pointer to it into register `a1`.
1. `arch/riscv/entry.S` sets up the stack pointer for all harts in assembly, then jumps to C. It also checks if the PC has more CPUs as are being supported and sends the extra ones into an infinite loop.
2. `arch/riscv/start.c` sets up the remaining RISC V specific bits in [S-mode](../../riscv/S-mode.md).
3. `init/main.c` is the architecture independent starting point. This is where the kernel does the actual init of the subsystems (virtual memory, console output, interrupts etc.). The first hart will init SBI and the Timer Interrupts (see [interrupts](../interrupts/interrupts.md)). If the `hart state management` SBI extension is found, additional harts can be started (they also start in `arch/riscv/entry.S`.


## Init

See [init_overview](init_overview.md) for the OS and [user space](../../userspace/userspace.md) [init](../processes/init_userspace.md) process.


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](boot_process.md) | [init_overview](init_overview.md)

**See also:** [life_cycle_cpu](life_cycle_cpu.md) [life_cycle_user_application](life_cycle_user_application.md)
