/* SPDX-License-Identifier: MIT */
#pragma once

#ifndef __ASSEMBLY__
#include <arch/fence.h>
#include <arch/riscv/asm/satp.h>
#include <kernel/kernel.h>

//
// xlen is the RISC-V register width
// xlen_t is used where data is load/stored to registers
// it has the same size as size_t
//
typedef size_t xlen_t;

#endif  // __ASSEMBLY__

#if defined(__ARCH_32BIT)
#define HIGHEST_BIT (1L << 31)
#else
#define HIGHEST_BIT (1L << 63)
#endif

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

// Supervisor Interrupt Pending, sip csr uses same bits as sie csr!
#define SIP_SEIP SIE_SEIE  // external
#define SIP_STIP SIE_STIE  // timer
#define SIP_SSIP SIE_SSIE  // software

/// physical memory protection CSR read access
#define PMP_R (1L << 0)
/// physical memory protection CSR write access
#define PMP_W (1L << 1)
/// physical memory protection CSR execute access
#define PMP_X (1L << 2)
/// physical memory protection CSR naturally aligned power of two
#define PMP_MATCH_NAPOT (3L << 3)

#define PMP_RANGE_BOTTOM 0
#if (__riscv_xlen == 32)
#define PMP_RANGE_TOP 0xffffffff
#else
#define PMP_RANGE_TOP 0x3fffffffffffffull
#endif

#ifndef __ASSEMBLY__

// read CSR:
#define rv_read_csr_(name)                                      \
    static inline xlen_t rv_read_csr_##name()                   \
    {                                                           \
        xlen_t x;                                               \
        asm volatile("csrr %0, " #name : "=r"(x) : : "memory"); \
        return x;                                               \
    }

// write CSR:
#define rv_write_csr_(name)                                       \
    static inline void rv_write_csr_##name(xlen_t x)              \
    {                                                             \
        asm volatile("csrw " #name ", %0" : : "r"(x) : "memory"); \
    }

// set bits in CSR:
#define rv_set_csr_(name)                                          \
    static inline void rv_set_csr_##name(xlen_t x)                 \
    {                                                              \
        asm volatile("csrs " #name ", %0" : : "rK"(x) : "memory"); \
    }

// read old CSR value and set bits:
#define rv_read_set_csr_(name)                               \
    static inline xlen_t rv_read_set_csr_##name(xlen_t flag) \
    {                                                        \
        xlen_t x;                                            \
        asm volatile("csrrs %0, " #name ", %1"               \
                     : "=r"(x)                               \
                     : "rK"(flag)                            \
                     : "memory");                            \
        return x;                                            \
    }

// clear bits in CSR:
#define rv_clear_csr_(name)                                        \
    static inline void rv_clear_csr_##name(xlen_t x)               \
    {                                                              \
        asm volatile("csrc " #name ", %0" : : "rK"(x) : "memory"); \
    }

// read old CSR value and clear bits:
#define rv_read_clear_csr_(name)                               \
    static inline xlen_t rv_read_clear_csr_##name(xlen_t flag) \
    {                                                          \
        xlen_t x;                                              \
        asm volatile("csrrc %0, " #name ", %1"                 \
                     : "=r"(x)                                 \
                     : "rK"(flag)                              \
                     : "memory");                              \
        return x;                                              \
    }

//
// supervisor mode (OS mode)
rv_read_csr_(sstatus);
rv_write_csr_(sstatus);
rv_set_csr_(sstatus);
rv_clear_csr_(sstatus);

rv_read_csr_(sip);  // Supervisor Interrupt Pending
rv_write_csr_(sip);
rv_set_csr_(sip);
rv_clear_csr_(sip);

rv_read_csr_(sie);  // Supervisor Interrupt Enable
rv_write_csr_(sie);
rv_set_csr_(sie);
rv_clear_csr_(sie);

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

// Time CSRs: it's a 64bit value so 32bit mode needs two CSRs to get the full
// value
rv_read_csr_(time);

#if (__riscv_xlen == 32)
rv_read_csr_(timeh);

static inline uint64_t rv_get_time()
{
    uint32_t time_high_0;
    uint32_t time_low;
    uint32_t time_high_1;

    do {
        // read the high value twice to detect an overflow
        time_high_0 = rv_read_csr_timeh();
        time_low = rv_read_csr_time();
        time_high_1 = rv_read_csr_timeh();
    } while (time_high_0 != time_high_1);

    return ((uint64_t)time_high_0 << 32) + (uint64_t)time_low;
}
#else
static inline uint64_t rv_get_time() { return rv_read_csr_time(); }
#endif

// read cycle counter
rv_read_csr_(cycle);
#if (__riscv_xlen == 32)
rv_read_csr_(cycleh);
#endif

#if (__riscv_xlen == 32)
/// @brief get current cycle count as 64 bit value
static inline uint64_t rv_get_cycles()
{
    uint32_t cycle_high_0;
    uint32_t cycle_low;
    uint32_t cycle_high_1;

    do {
        // read the high value twice to detect an overflow
        cycle_high_0 = rv_read_csr_cycleh();
        cycle_low = rv_read_csr_cycle();
        cycle_high_1 = rv_read_csr_cycleh();
    } while (cycle_high_0 != cycle_high_1);

    return ((uint64_t)cycle_high_0 << 32) + (uint64_t)cycle_low;
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
    rv_write_csr_stimecmph(high);
    rv_write_csr_stimecmp(low);
}
#else
/// @brief get Sstc stimecmp register
static inline uint64_t rv_get_stimecmp() { return rv_read_csr_stimecmp(); }
/// @brief set Sstc stimecmp register
static inline void rv_set_stimecmp(uint64_t new_value)
{
    return rv_write_csr_stimecmp(new_value);
}
#endif  // __riscv_xlen

#endif  // __RISCV_EXT_SSTC

#endif  // __ASSEMBLY__
