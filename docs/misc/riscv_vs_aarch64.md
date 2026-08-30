# RISC V vs AArch64

This page should serve as a quick look-up guide for the main assembly level differences between the supported architectures.

## Registers

| Purpose                        | RISC-V RV64                   | AArch64                                                  |
| ------------------------------ | ----------------------------- | -------------------------------------------------------- |
| Zero register                  | `x0` (`zero`)                 | `xzr`                                                    |
| Return address / link register | `x1` (`ra`)                   | `x30` (`lr`)                                             |
| Stack pointer                  | `x2` (`sp`)                   | `sp`                                                     |
| Frame pointer                  | `x8` (`s0/fp`)                | `x29` (`fp`)                                             |
| Thread pointer                 | `x4` (`tp`)                   | platform-specific (`tpidr_el0` system register, not GPR) |
| Global pointer                 | `x3` (`gp`)                   | none                                                     |
| Function arguments             | `x10–x17` (`a0–a7`)           | `x0–x7`                                                  |
| Function return values         | `x10–x11` (`a0–a1`)           | `x0–x1`                                                  |
| Caller-saved temporaries       | `x5–x7`, `x28–x31` (`t0–t6`)  | `x9–x15`                                                 |
| Intra-procedure scratch / PLT  | none                          | `x16–x17` (`ip0–ip1`)                                    |
| Platform register              | none                          | `x18`                                                    |
| Callee-saved registers         | `x8–x9`, `x18–x27` (`s0–s11`) | `x19–x29`                                                |

## MMU

There is also Sv48 and support for larger addess spaces on ARM. The list below focuses on the variants supported by VIMIX.

| Feature                           | RISC-V Sv32               | RISC-V Sv39                               | AArch64 (with 48-bit VA)                          |
| --------------------------------- | ------------------------- | ----------------------------------------- | ------------------------------------------------- |
| Architecture width                | 32 bit                    | 64 bit                                    | 64 bit                                            |
| Virtual address size              | 32 bit                    | 39 bit                                    | 48 bit                                            |
| Usable bits                       | all 32                    | 39 (sign extension from bit 38)           | 48 (sign extension from bit 47)                   |
| User virtual address range        | `0x00000000 – 0x7FFFFFFF` | `0x0000000000000000 – 0x0000003FFFFFFFFF` | `0x0000000000000000 – 0x0000FFFFFFFFFFFF`         |
| Kernel virtual address range      | `0x80000000 – 0xFFFFFFFF` | `0xFFFFFFC000000000 – 0xFFFFFFFFFFFFFFFF` | `0xFFFF000000000000 – 0xFFFFFFFFFFFFFFFF`         |
| Total virtual address space       | 4 GiB                     | 512 GiB                                   | 256 TiB                                           |
| Page size                         | 4 KiB                     | 4 KiB                                     | 4 KiB (16K/64K possible but not supported)        |
| Large page support                | 4 MiB                     | 2 MiB, 1 GiB                              | 2 MiB, 1 GiB (assuming 4KiB page size)            |
| Page table levels                 | 2                         | 3                                         | 4                                                 |
| Entries per table                 | 1024                      | 512                                       | 512                                               |
| PTE size                          | 4 bytes                   | 8 bytes                                   | 8 bytes                                           |
| Page table indexing               | 10/10/12                  | 9/9/9/12                                  | 9/9/9/9/12                                        |
| Address translation base register | `satp`                    | `satp`                                    | user space: `TTBR0_EL1`, kernel space:`TTBR1_EL1` |
| ASIDs                             | 65.535                    | 65.535                                    | 255 or 65.535                                     |

## CPU Run Levels

| Concept                        | RISC-V  | AArch64 | In VIMIX                               |
| ------------------------------ | ------- | ------- | -------------------------------------- |
| User mode                      | U-mode  | EL0     | [userspace](../userspace/userspace.md) |
| Kernel / OS mode               | S-mode  | EL1     | [kernel](../kernel/kernel.md)          |
| Hypervisor mode                | HS-mode | EL2     | -                                      |
| Secure monitor / firmware mode | M-mode  | EL3     | optional on RISC V                     |
| Firmware                       | SBI     | PSCI    | RISC V expects [SBI](../riscv/SBI.md)  |

## Exception Registers

| Purpose                          | AArch64                  | RISC-V                                       |
| -------------------------------- | ------------------------ | -------------------------------------------- |
| Exception vector base            | `VBAR_EL1`               | `stvec`                                      |
| Machine/firmware vector base     | `VBAR_EL3`               | `mtvec`                                      |
| Saved exception PC               | `ELR_EL1`                | `sepc`                                       |
| Machine/firmware saved PC        | `ELR_EL3`                | `mepc`                                       |
| Saved processor state            | `SPSR_EL1`               | `sstatus`                                    |
| Machine/firmware saved state     | `SPSR_EL3`               | `mstatus`                                    |
| Exception cause register         | `ESR_EL1`                | `scause`                                     |
| Machine/firmware exception cause | `ESR_EL3`                | `mcause`                                     |
| Fault address register           | `FAR_EL1`                | `stval`                                      |
| Machine/firmware fault address   | `FAR_EL3`                | `mtval`                                      |
| Translation table base           | `TTBR0_EL1`, `TTBR1_EL1` | `satp`                                       |
| MMU configuration                | `TCR_EL1`                | `satp` + mode CSRs                           |
| System control register          | `SCTLR_EL1`              | `sstatus` / `mstatus`                        |
| Interrupt mask/control           | `DAIF`                   | interrupt enable bits in `sstatus`/`mstatus` |
| Timer control                    | `CNT*` registers         | `stimecmp`, `mtimecmp`                       |
| Trap delegation                  | mostly fixed routing     | `medeleg`, `mideleg`                         |
| Exception return instruction     | `eret`                   | `sret`, `mret`                               |
| Syscall instruction              | `svc #imm`               | `ecall`                                      |

---
**Up:** [architectures](architectures.md)

**Achitectures:** [ARM 64](../aarch64/ARM%2064.md) | [RISC V](../riscv/RISCV.md)
