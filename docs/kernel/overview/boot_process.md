# Boot Process - How to get from Assembly to main()

The [kernel](../kernel.md) gets compiled into an ELF binary, then gets striped by `objdump`. This leaves the binary format as been defined in the linker script without any additional headers. The first program code to be placed in the binary is located in `arch/<ARCHITECTURE>/asm/head.S` and contains a header defined in assembly. This mimics the header Linux uses to allow existing boot loaders to load the kernel.

The initial boot code is position independent and thus does not care where the boot loader loaded the kernel into memory. This limits early C code to not access global variables.

First `_start` in `arch/riscv/asm/head.S` is called. On [RISC V](../../riscv/RISCV.md) bare metal with all harts initially, but one gets selected as the boot hart via a lottery. Environments with a firmware ([RISCV](../../riscv/RISCV.md) with [SBI](../../riscv/SBI.md) or [ARM 64](../../aarch64/ARM%2064.md)) boot with just one.

First the kernels BSS memory section is cleared.

Next a stack is created to jump to C into `early_pgtable_init`. The goal is to generate a page table that maps the kernel and a bit of memory from the loaded physical location into a virtual location in the upper half of the address space. As the kernel is linked into this upper half, the code to generate the early page table must be location independent and can not call kmalloc.

After enabling the page table, RISC V `M-Mode` code on bare metal takes a detour:

0. `arch/riscv/m_mode.S` sets up a stack per CPU, calls `m_mode_start()` and picks a boot hart via a lottery. The non-boot harts will wait for an interrupt, which gets triggered when the boot hart starts these harts vi [SBI](../../riscv/SBI.md) later.
1. `arch/riscv/m_mode.c` sets up the remaining RISC V specific bits in [M-mode](../../riscv/M-mode.md) and registers an interrupt handler for the timer (not started yet), illegale instructions (to emulate time/timeh csrs) and [SBI](../../riscv/SBI.md) ecalls.
2. After this setup only the boot hart jumps to `_entry_s_mode_boot_hart` in [S-Mode](../../riscv/S-mode.md). The remaining boot is identical to booting in a real [SBI](../../riscv/SBI.md) environment (except for the linker address).

Next `_entry_s_mode_boot_hart`  will setup a stack and jump to `main()` in `init/main.c`. This is the architecture independent starting point. First it tries to init a way for `printk` to print and the memory management to allow `kmalloc()`. Then [devices](../devices/devices.md), [file systems](../file_system/file_system.md) and [processes](../processes/processes.md) are initialized.

If the `hart state management` [SBI](../../riscv/SBI.md) extension is found, additional harts can be started (they also start in `arch/riscv/head.S` but in `_start_secondary` where they set the current page table and enter `main()`). The same is done on [ARM 64](../../aarch64/ARM%2064.md) via platform specific methods.

## Init

See [init_overview](init_overview.md) for the OS and [user space](../../userspace/userspace.md) [init](../processes/init_userspace.md) process.

---
**Overview:** [kernel](../kernel.md)

**Boot:** [boot_process](boot_process.md) | [init_overview](init_overview.md)

**See also:** [life_cycle_cpu](life_cycle_cpu.md) [life_cycle_user_application](life_cycle_user_application.md)
