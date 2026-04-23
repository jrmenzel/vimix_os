# Boot Process - How to get from Assembly to main()

On [RISC V](../../riscv/RISCV.md) the [kernel](../kernel.md) expects to be loaded at address `0x80000000` on bare metal and address `0x80200000` when running under [SBI](../../riscv/SBI.md). This is defined in the kernels linker script.

In all cases, `_entry` in `arch/riscv/asm/entry.S` is called first. On bare metal with all harts initially, but one gets selected as the boot hart via a lottery. SBI boots with just one.

First the kernels BSS memory section is cleared.

Next a stack is created to jump to C into `early_pgtable_init`. The goal is to generate a page table that maps the kernel and a bit of memory from the loaded physical location into a virtual location in the upper half of the address space. As the kernel is linked into this upper half, the code to generate the early page table must be location independent and can not call kmalloc.

After enabling the page table, `M-Mode` code on bare metal takes a detour:

0. `arch/riscv/m_mode.S` sets up a stack per CPU, calls `m_mode_start()` and picks a boot hart via a lottery. The non-boot harts will wait for an interrupt, which gets triggered when the boot hart starts these harts vi [SBI](../../riscv/SBI.md) later.
1. `arch/riscv/m_mode.c` sets up the remaining RISC V specific bits in [M-mode](../../riscv/M-mode.md) and registers an interrupt handler for the timer (not started yet), illegale instructions (to emulate time/timeh csrs) and [SBI](../../riscv/SBI.md) ecalls.
2. After this setup only the boot hart jumps to `_entry_s_mode_boot_hart` in [S-Mode](../../riscv/S-mode.md). The remaining boot is identical to booting in a real [SBI](../../riscv/SBI.md) environment (except for the linker address).

Next `_entry_s_mode_boot_hart`  will setup a stack and jump to `main()` in `init/main.c`. This is the architecture independent starting point. First it tries to init a way for `printk` to print and the memory management to allow `kmalloc()`. Then [devices](../devices/devices.md), [file systems](../file_system/file_system.md) and [processes](../processes/processes.md) are initialized.

If the `hart state management` SBI extension is found, additional harts can be started (they also start in `arch/riscv/entry.S` but in `_entry_s_mode` where they set the current page table and enter `main()`).


## Init

See [init_overview](init_overview.md) for the OS and [user space](../../userspace/userspace.md) [init](../processes/init_userspace.md) process.


---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](boot_process.md) | [init_overview](init_overview.md)

**See also:** [life_cycle_cpu](life_cycle_cpu.md) [life_cycle_user_application](life_cycle_user_application.md)
