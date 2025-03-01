/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/cpu.h>
#include <kernel/types.h>

/// unique ID of the processor / CPU. E.g. HART ID on RISC-V
/// Must be called with interrupts disabled,
/// to prevent race with process being moved
/// to a different CPU.
/// Implemented per CPU architecture.
#define smp_processor_id ___ARCH_smp_processor_id
