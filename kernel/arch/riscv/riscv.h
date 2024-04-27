/* SPDX-License-Identifier: MIT */
#pragma once

#ifndef __ASSEMBLER__

#include <kernel/types.h>

//
// xlen is the RISC-V register width
// xlen_t is used where data is load/stored to registers
// it has the same size as size_t
//
typedef size_t xlen_t;

/// which hart (core) is this?
static inline xlen_t r_mhartid()
{
    xlen_t x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11)  // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)  // machine-mode interrupt enable.

static inline xlen_t r_mstatus()
{
    xlen_t x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
}

static inline void w_mstatus(xlen_t x)
{
    asm volatile("csrw mstatus, %0" : : "r"(x));
}

/// machine exception program counter, holds the
/// instruction address to which a return from
/// exception will go.
static inline void w_mepc(xlen_t x)
{
    asm volatile("csrw mepc, %0" : : "r"(x));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)   // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5)  // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4)  // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)   // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)   // User Interrupt Enable

static inline xlen_t r_sstatus()
{
    xlen_t x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

static inline void w_sstatus(xlen_t x)
{
    asm volatile("csrw sstatus, %0" : : "r"(x));
}

/// Supervisor Interrupt Pending
static inline xlen_t r_sip()
{
    xlen_t x;
    asm volatile("csrr %0, sip" : "=r"(x));
    return x;
}

static inline void w_sip(xlen_t x) { asm volatile("csrw sip, %0" : : "r"(x)); }

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9)  // external
#define SIE_STIE (1L << 5)  // timer
#define SIE_SSIE (1L << 1)  // software
static inline xlen_t r_sie()
{
    xlen_t x;
    asm volatile("csrr %0, sie" : "=r"(x));
    return x;
}

static inline void w_sie(xlen_t x) { asm volatile("csrw sie, %0" : : "r"(x)); }

// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11)  // external
#define MIE_MTIE (1L << 7)   // timer
#define MIE_MSIE (1L << 3)   // software
static inline xlen_t r_mie()
{
    xlen_t x;
    asm volatile("csrr %0, mie" : "=r"(x));
    return x;
}

static inline void w_mie(xlen_t x) { asm volatile("csrw mie, %0" : : "r"(x)); }

/// supervisor exception program counter, holds the
/// instruction address to which a return from
/// exception will go.
static inline void w_sepc(xlen_t x)
{
    asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline xlen_t r_sepc()
{
    xlen_t x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

/// Machine Exception Delegation
static inline xlen_t r_medeleg()
{
    xlen_t x;
    asm volatile("csrr %0, medeleg" : "=r"(x));
    return x;
}

static inline void w_medeleg(xlen_t x)
{
    asm volatile("csrw medeleg, %0" : : "r"(x));
}

/// Machine Interrupt Delegation
static inline xlen_t r_mideleg()
{
    xlen_t x;
    asm volatile("csrr %0, mideleg" : "=r"(x));
    return x;
}

static inline void w_mideleg(xlen_t x)
{
    asm volatile("csrw mideleg, %0" : : "r"(x));
}

/// Supervisor Trap-Vector Base Address
/// low two bits are mode.
static inline void w_stvec(xlen_t x)
{
    asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline xlen_t r_stvec()
{
    xlen_t x;
    asm volatile("csrr %0, stvec" : "=r"(x));
    return x;
}

/// Machine-mode interrupt vector
static inline void w_mtvec(xlen_t x)
{
    asm volatile("csrw mtvec, %0" : : "r"(x));
}

/// Physical Memory Protection
static inline void w_pmpcfg0(xlen_t x)
{
    asm volatile("csrw pmpcfg0, %0" : : "r"(x));
}

static inline void w_pmpaddr0(xlen_t x)
{
    asm volatile("csrw pmpaddr0, %0" : : "r"(x));
}

/// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64_t)pagetable) >> 12))

/// supervisor address translation and protection;
/// holds the address of the page table.
static inline void cpu_set_page_table(xlen_t addr)
{
    asm volatile("csrw satp, %0" : : "r"(addr));
}

static inline xlen_t r_satp()
{
    xlen_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

static inline void w_mscratch(uint64_t x)
{
    asm volatile("csrw mscratch, %0" : : "r"(x));
}

/// Supervisor Trap Cause
static inline xlen_t r_scause()
{
    xlen_t x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
}

/// Supervisor Trap Value
static inline xlen_t r_stval()
{
    xlen_t x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
}

/// Machine-mode Counter-Enable
static inline void w_mcounteren(xlen_t x)
{
    asm volatile("csrw mcounteren, %0" : : "r"(x));
}

static inline xlen_t r_mcounteren()
{
    xlen_t x;
    asm volatile("csrr %0, mcounteren" : "=r"(x));
    return x;
}

/// machine-mode cycle counter
static inline xlen_t r_time()
{
    xlen_t x;
    asm volatile("csrr %0, time" : "=r"(x));
    return x;
}

static inline xlen_t r_sp()
{
    xlen_t x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

static inline void w_tp(xlen_t x) { asm volatile("mv tp, %0" : : "r"(x)); }

static inline xlen_t r_ra()
{
    xlen_t x;
    asm volatile("mv %0, ra" : "=r"(x));
    return x;
}

/// flush the TLB.
static inline void rv_sfence_vma()
{
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
}

#endif  // __ASSEMBLER__
