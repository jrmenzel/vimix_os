/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief Clears the BSS section (uninitialized and Zero-initialized C
/// variables) with zeros.
///        All kernel threads should call this as early as possible (before
///        reading or writing any variables that could be in BSS), but only one
///        should perform the clear. All other threads will wait.
///        Note that the kernel stack is not in bss (see kernel.ld) - if it
///        would be, the bss clear would have to be done from assembly before
///        jumping to C.
/// @param this_thread_clears Should be true for only one thread.
void wait_on_bss_clear(bool this_thread_clears);

/// @brief entry.S jumps here in Kernel Mode (when run on SBI or ARM) or Machine
///        Mode otherwise. Stack is on g_kernel_cpu_stack[KERNEL_STACK_SIZE *
///        cpu_id]. In RISC V M-Mode all cores start at the same time, pick ID 0
///        as the main thread. On SBI only one hart starts initially, all other
///        harts are started explicitly via init_platform() - but those also
///        call _entry->start().
/// @param cpuid Hart ID
/// @param device_tree set by SBI to the device tree file, not set for cores
/// started by sbi_hart_start()
void start(size_t cpuid, void *device_tree);

//
// to implement per ARCH:
//

void cpu_set_boot_state();
