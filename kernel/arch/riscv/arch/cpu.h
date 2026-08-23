/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <init/start.h>
#include <kernel/kernel.h>
#include <mm/arch_vm.h>
#include <mm/memlayout.h>

typedef uint32_t CPU_Features;
#define RV_SV32_SUPPORTED 0x01
#define RV_SV39_SUPPORTED 0x02
#define RV_SV48_SUPPORTED 0x04
#define RV_SV57_SUPPORTED 0x08
#define RV_EXT_FLOAT 0x10
#define RV_EXT_DOUBLE 0x20
#define RV_EXT_SSTC 0x40

/// read and write tp, the thread pointer, which VIMIX uses to hold
/// this core's hartid (core number), the index into g_cpus[].
/// This is required as the Hart ID can only be read in Machine Mode, but
/// the OS needs to know the ID in Supervisor Mode.
static inline size_t __arch_smp_processor_id()
{
    size_t x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

/// @brief which interrupts are reported IF interrupts are globaly enabled
static inline void cpu_set_interrupt_mask()
{
    // enable external, timer, software interrupts
    rv_set_csr_sie(SIE_SEIE | SIE_STIE | SIE_SSIE);
}

/// enable device interrupts
static inline void cpu_enable_interrupts() { rv_set_csr_sstatus(SSTATUS_SIE); }

/// disable device interrupts
static inline void cpu_disable_interrupts()
{
    rv_clear_csr_sstatus(SSTATUS_SIE);
}

/// are device interrupts enabled?
static inline bool cpu_is_interrupts_enabled()
{
    xlen_t x = rv_read_csr_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

/// Set the Supervisor-mode trap vector (interrupt handler) function
static inline void cpu_set_trap_vector(size_t supervisor_trap_vector)
{
    rv_write_csr_stvec((xlen_t)supervisor_trap_vector);
}

/// @brief let the CPU sleep until the next interrupt occurs
static inline void wait_for_interrupt() { asm volatile("wfi"); }

extern char u_mode_trap_vector[];
static inline void cpu_prepare_return_to_user_mode()
{
    // set S Previous Privilege mode to User.
    xlen_t x = rv_read_csr_sstatus();
    x &= ~SSTATUS_SPP;  // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE;  // enable interrupts in user mode
    rv_write_csr_sstatus(x);

    // send syscalls, interrupts, and exceptions to u_mode_trap_vector in
    // u_mode_trap_vector.S
    size_t trampoline_u_mode_trap_vector = (size_t)u_mode_trap_vector;

    cpu_set_trap_vector(trampoline_u_mode_trap_vector);
}

static inline void cpu_set_exception_return_address(size_t addr)
{
    rv_write_csr_sepc(addr);
}

static inline void cpu_set_user_stack_pointer(size_t sp)
{
    asm volatile("csrw sscratch, %0" : : "r"(sp) : "memory");
}

static inline size_t cpu_get_next_inst_after_syscall(size_t syscall_pc)
{
    // RISC-V ecall is 4 bytes, so the next instruction is always 4 bytes after.
    return syscall_pc + 4;
}

// no explicit flush needed on RISC V
static inline void cpu_flush_dcache_range(size_t va, size_t len) {}

/// call after changing executable code in memory to flush instruction caches
CAN_BE_CALLED_ON_USER_PAGE_TABLE static inline void
cpu_flush_instruction_cache()
{
    asm volatile("fence.i");
}
