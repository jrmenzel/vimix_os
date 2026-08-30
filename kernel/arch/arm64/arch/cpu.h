/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/arm64/arm64.h>
#include <arch/barrier.h>
#include <kernel/kernel.h>
#include <kernel/param.h>

// Can't send IPIs to more than 8 CPUs with GICv2 SGI
_Static_assert(MAX_CPUS <= 8,
               "MAX_CPUS too large for ARM 64 supported by GICv2 SGI");

typedef struct
{
    size_t icache_line_size_bytes;
    size_t dcache_line_size_bytes;
    bool idc_flag;
    bool dic_flag;
} CPU_Features;

static inline size_t __arch_smp_processor_id() { return arm_read_tpidr_el1(); }

static inline void cpu_set_interrupt_mask()
{
    asm volatile("msr daifset, #0x1" ::: "memory");
}

static inline void cpu_enable_interrupts()
{
    // 2 = only enable IRQs
    asm volatile("msr daifclr, #0x2" ::: "memory");
}

static inline void cpu_disable_interrupts()
{
    asm volatile("msr daifset, #0x3" ::: "memory");
}

/// are device interrupts enabled?
static inline bool cpu_is_interrupts_enabled()
{
    size_t x = arm_read_daif();
    return ((x & DAIF_STATE_IRQ) == 0);
}

/// Set the kernel trap vector (interrupt handler) function
static inline void cpu_set_trap_vector(size_t supervisor_trap_vector)
{
    arm_write_vbar_el1(supervisor_trap_vector);
    isb();
}

/// @brief let the CPU sleep until the next interrupt occurs
static inline void wait_for_interrupt() { asm volatile("wfi"); }

static inline void cpu_prepare_return_to_user_mode()
{
    // Return to EL0 with IRQ enabled but FIQ masked.
    const size_t irq_enabled = (1UL << 9) | (1UL << 8) | (1UL << 6);
    arm_write_spsr_el1(irq_enabled);
}

static inline void cpu_set_exception_return_address(size_t addr)
{
    arm_write_elr_el1(addr);
}

static inline void cpu_set_user_stack_pointer(size_t sp)
{
    arm_write_sp_el0(sp);
}

static inline size_t cpu_get_next_inst_after_syscall(size_t syscall_pc)
{
    // ARM64 PC is already increased
    return syscall_pc;
}

void cpu_flush_dcache_range(size_t va, size_t len);

/// call after changing executable code in memory to flush instruction caches
static inline void cpu_flush_instruction_cache()
{
    asm volatile("ic iallu");
    dsb(ish);
    isb();
}
