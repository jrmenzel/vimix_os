/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/fence.h>
#include <kernel/types.h>

#if !defined(__RISCV_CSR_TIME)
#include <kernel/kticks.h>
#include <timer.h>
#endif

//
// xlen is the RISC-V register width
// xlen_t is used where data is load/stored to registers
// it has the same size as size_t
//
typedef size_t xlen_t;

// mandatory extensions
#define __RISCV_EXT_ZICSR
#define __RISCV_EXT_ZIFENCEI

// Supervisor Status Register, sstatus
#define SSTATUS_SPP (1L << 8)   // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5)  // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4)  // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)   // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)   // User Interrupt Enable

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9)  // external
#define SIE_STIE (1L << 5)  // timer
#define SIE_SSIE (1L << 1)  // software

#ifdef CONFIG_RISCV_BOOT_M_MODE
// (When running in an SBI environment the kernel has no
// direct access to M-Mode. Hide all M-Mode defines to prevent accidental
// usage as it will result in runtime issues.)

// Machine Status Register, mstatus
#define MSTATUS_MPP_MASK (3L << 11)  // previous mode mask.
#define MSTATUS_MPP_M (3L << 11)     // mret will go to Machine Mode
#define MSTATUS_MPP_S (1L << 11)     // mret will go to Supervisor Mode
#define MSTATUS_MPP_U (0L << 11)     // mret will go to User Mode
#define MSTATUS_MIE (1L << 3)        // machine-mode interrupt enable.

// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11)  // external
#define MIE_MTIE (1L << 7)   // timer
#define MIE_MSIE (1L << 3)   // software
#define MIE_STIE (1L << 5)   // supervisor timer
#endif                       // CONFIG_RISCV_BOOT_M_MODE

#if defined(__ARCH_is_32bit)
// use riscv's sv32 page table scheme.
#define SATP_SV32 (1L << 31)
#define MAKE_SATP(pagetable) (SATP_SV32 | (((uint32_t)pagetable) >> 12))
#else
// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)
#define SATP_SV48 (9L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64_t)pagetable) >> 12))
#endif  // __ARCH_is_32bit

/// physical memory protection CSR read access
#define PMP_R (1L << 0)
/// physical memory protection CSR write access
#define PMP_W (1L << 1)
/// physical memory protection CSR execute access
#define PMP_X (1L << 2)
/// physical memory protection CSR naturally aligned power of two
#define PMP_MATCH_NAPOT (3L << 3)

// CSR read macro:
#define rv_read_csr_(name)                         \
    static inline xlen_t rv_read_csr_##name()      \
    {                                              \
        xlen_t x;                                  \
        asm volatile("csrr %0, " #name : "=r"(x)); \
        return x;                                  \
    }

// CSR write macro:
#define rv_write_csr_(name)                            \
    static inline void rv_write_csr_##name(xlen_t x)   \
    {                                                  \
        asm volatile("csrw " #name ", %0" : : "r"(x)); \
    }

#ifdef CONFIG_RISCV_BOOT_M_MODE
// machine mode CSR read / write functions

rv_read_csr_(mhartid);  // hard-id

rv_read_csr_(pmpcfg0);  // Physical Memory Protection
rv_write_csr_(pmpcfg0);

rv_read_csr_(mstatus);
rv_write_csr_(mstatus);

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
rv_write_csr_(mepc);

rv_read_csr_(mie);  // machine-mode Interrupt Enable
rv_write_csr_(mie);

// Machine Exception Delegation
rv_read_csr_(medeleg);
rv_write_csr_(medeleg);

// Machine Interrupt Delegation
rv_read_csr_(mideleg);
rv_write_csr_(mideleg);

rv_read_csr_(mscratch);
rv_write_csr_(mscratch);

// Machine mode trap vectors
rv_read_csr_(mtvec);
rv_write_csr_(mtvec);

// Machine-mode Counter-Enable
rv_read_csr_(mcounteren);
rv_write_csr_(mcounteren);

// physical memory protection register 0
rv_write_csr_(pmpaddr0);

// environment config register
rv_read_csr_(menvcfg);
rv_write_csr_(menvcfg);
#if (__riscv_xlen == 32)
rv_read_csr_(menvcfgh);
rv_write_csr_(menvcfgh);
#endif

#endif  // CONFIG_RISCV_BOOT_M_MODE

//
// supervisor mode (OS mode)
rv_read_csr_(sstatus);
rv_write_csr_(sstatus);

rv_read_csr_(sip);  // Supervisor Interrupt Pending
rv_write_csr_(sip);

rv_read_csr_(sie);  // Supervisor Interrupt Enable
rv_write_csr_(sie);

// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
rv_read_csr_(sepc);
rv_write_csr_(sepc);

// Supervisor Trap-Vector Base Address
// low two bits are mode.
rv_read_csr_(stvec);
rv_write_csr_(stvec);

// supervisor address translation and protection;
// holds the address of the page table.
rv_read_csr_(satp);
rv_write_csr_(satp);

// Supervisor Trap Cause
rv_read_csr_(scause);

// Supervisor Trap Value
rv_read_csr_(stval);

#if defined(__RISCV_CSR_TIME)
// Time CSRs: it's a 64bit value so 32bit mode needs two CSRs to get the full
// value
rv_read_csr_(time);
#if (__riscv_xlen == 32)
rv_read_csr_(timeh);
#endif

#if (__riscv_xlen == 32)
static inline uint64_t rv_get_time()
{
    uint32_t time_low = rv_read_csr_time();
    uint32_t time_high = rv_read_csr_timeh();

    return ((uint64_t)time_high << 32) + (uint64_t)time_low;
}
#else
static inline uint64_t rv_get_time() { return rv_read_csr_time(); }
#endif

#else   // __RISCV_CSR_TIME
static inline uint64_t rv_get_time()
{
    // hack for platforms without time CSRs (Spike without SBI)
    uint64_t now =
        kticks_get_ticks() * g_timebase_frequency / TIMER_INTERRUPTS_PER_SECOND;
    return now;
}
#endif  // __RISCV_CSR_TIME

// read cycle counter
rv_read_csr_(cycle);
#if (__riscv_xlen == 32)
rv_read_csr_(cycleh);
#endif

#if (__riscv_xlen == 32)
/// @brief get current cycle count as 64 bit value
static inline uint64_t rv_get_cycles()
{
    uint32_t cycle_low = rv_read_csr_cycle();
    uint32_t cycle_high = rv_read_csr_cycleh();
    return ((uint64_t)cycle_high << 32) + (uint64_t)cycle_low;
}
#else
/// @brief get current cycle count as 64 bit value
static inline uint64_t rv_get_cycles() { return rv_read_csr_cycle(); }
#endif

// read instruction retired counter
rv_read_csr_(instret);
#if (__riscv_xlen == 32)
rv_read_csr_(instreth);
#endif

#if (__riscv_xlen == 32)
/// @brief get instruction retired counter
static inline uint64_t rv_get_instret()
{
    uint32_t instret_low = rv_read_csr_instret();
    uint32_t instret_high = rv_read_csr_instreth();
    return ((uint64_t)instret_high << 32) + (uint64_t)instret_low;
}
#else
/// @brief get instruction retired counter
static inline uint64_t rv_get_instret() { return rv_read_csr_instret(); }
#endif

//
// Sstc Extension
//
#if defined(__RISCV_EXT_SSTC)

/// @brief stime compare register for extension Sstc
rv_read_csr_(stimecmp);
rv_write_csr_(stimecmp);
#if (__riscv_xlen == 32)
rv_read_csr_(stimecmph);
rv_write_csr_(stimecmph);
#endif

#if (__riscv_xlen == 32)
/// @brief get Sstc stimecmp register
static inline uint64_t rv_get_stimecmp()
{
    uint32_t low = rv_read_csr_stimecmp();
    uint32_t high = rv_read_csr_stimecmph();
    return ((uint64_t)high << 32) + (uint64_t)low;
}
/// @brief set Sstc stimecmp register
static inline void rv_set_stimecmp(uint64_t new_value)
{
    uint32_t low = new_value & 0xFFFFFFFF;
    uint32_t high = new_value >> 32;
    rv_write_csr_stimecmp(low);
    rv_write_csr_stimecmph(high);
}
#else
/// @brief get Sstc stimecmp register
static inline uint64_t rv_get_stimecmp() { return rv_read_csr_stimecmp(); }
/// @brief set Sstc stimecmp register
static inline void rv_set_stimecmp(uint64_t new_value)
{
    return rv_write_csr_stimecmp(new_value);
}
#endif

#endif

//
// zifencei Extension
//
#if defined(__RISCV_EXT_ZIFENCEI)
// flush the TLB.
static inline void rv_sfence_vma()
{
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
}
#endif

static inline void cpu_set_page_table(xlen_t addr)
{
    // wait for any previous writes to the page table memory to finish.
    rv_sfence_vma();

    rv_write_csr_satp(addr);

    // Depending on the cpu implementation a memory barrier might
    // not affect the instruction caches, so after loading executable
    // code an instruction memory barrier is needed.
    instruction_memory_barrier();

    // flush stale entries from the TLB.
    rv_sfence_vma();
}

static inline xlen_t cpu_get_page_table() { return rv_read_csr_satp(); }
