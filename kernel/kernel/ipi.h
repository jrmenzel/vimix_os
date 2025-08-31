/* SPDX-License-Identifier: MIT */
#pragma once

// Inter-Processor Interrupts

#include <kernel/kernel.h>
#include <kernel/param.h>
#include <kernel/smp.h>

#define MAX_IPI_PENDING 8

typedef uint64_t cpu_mask;
_Static_assert(sizeof(cpu_mask) * 8 >= MAX_CPUS,
               "cpu_mask too small for MAX_CPUS");

enum ipi_type
{
    IPI_NONE = 0,
    IPI_KERNEL_PANIC,
    IPI_SHUTDOWN,
    IPI_KERNEL_PAGETABLE_CHANGED,
};

/// @brief Call from boot CPU.
void ipi_init();

/// @brief Return a mask with all booted CPUs set
/// @return The mask.
cpu_mask ipi_cpu_mask_all();

/// @brief Return a mask with all booted CPUs set except the calling CPU.
/// @return The mask. Can be 0 if only one CPU is running.
static inline cpu_mask ipi_cpu_mask_all_but_self()
{
    cpu_mask mask = ipi_cpu_mask_all();
    cpu_mask self = (cpu_mask)1 << smp_processor_id();
    self = ~self;
    mask &= self;
    return mask;
}

/// @brief Send an IPI to the CPUs in the mask, not atomically.
/// @param mask Bit set for each CPU to send to, must be started.
/// @param type The IPI type to send.
/// @param data Depending on the type, may be NULL.
void ipi_send_interrupt(cpu_mask mask, enum ipi_type type, void* data);
