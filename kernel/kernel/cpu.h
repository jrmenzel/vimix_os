/* SPDX-License-Identifier: MIT */
#pragma once

// also include additional architecture-specific functions:
#include <arch/context.h>
#include <arch/cpu.h>
#include <kernel/ipi.h>

struct process;

enum CPU_State
{
    CPU_UNUSED = 0,
    CPU_STARTED,
    CPU_HALTED,
    CPU_PANICKED
};

/// Per-CPU state.
struct cpu
{
    enum CPU_State state;   ///< Boot state of this CPU.
    CPU_Features features;  ///< CPU features detected during boot.

    struct process *proc;    ///< The process running on this cpu, or null.
    struct context context;  ///< context_switch() here to enter scheduler().
    int32_t
        disable_dev_int_stack_depth;  ///< Depth of
                                      ///< cpu_push_disable_device_interrupt_stack()
                                      ///< nesting.
    bool
        disable_dev_int_stack_original_state;  ///< Were interrupts
                                               ///< enabled before
                                               ///< cpu_push_disable_device_interrupt_stack()?

    // Last global kernel page-table shootdown epoch this CPU has applied.
    size_t kernel_pgtable_epoch_seen;

    // Inter Processor Interrupts (IPI) data, access protected by
    // g_cpus_ipi_lock (one lock for all CPUs!).
    struct ipi
    {
        enum ipi_type pending;
        void *data;
    } ipi[MAX_IPI_PENDING];
};

extern struct cpu g_cpus[MAX_CPUS];
extern struct spinlock g_cpus_ipi_lock;

/// cpu_push_disable_device_interrupt_stack/cpu_pop_disable_device_interrupt_stack
/// are like cpu_disable_interrupts()/cpu_enable_interrupts()
/// except that they are matched: it takes two
/// cpu_pop_disable_device_interrupt_stack()s to undo two
/// cpu_push_disable_device_interrupt_stack()s.  Also, if interrupts are
/// initially off, then cpu_push_disable_device_interrupt_stack,
/// cpu_pop_disable_device_interrupt_stack leaves them off.
void cpu_push_disable_device_interrupt_stack();

/// See cpu_push_disable_device_interrupt_stack()
void cpu_pop_disable_device_interrupt_stack();
