/* SPDX-License-Identifier: MIT */

#include <arch/cpu.h>
#include <arch/start.h>
#include <init/dtb.h>
#include <init/main.h>
#include <init/start.h>
#include <kernel/kticks.h>
#include <kernel/string.h>
#include <mm/vm.h>

/// entry.S needs one kernel stack per CPU (one page of 4KB each)
/// As long as the kernel stack is fixed at 4K, recursion can be deadly.
__attribute__((
    aligned(PAGE_SIZE))) char g_kernel_cpu_stack[KERNEL_STACK_SIZE * MAX_CPUS];

volatile size_t g_kernel_init_status = KERNEL_INIT_EARLY;

size_t g_boot_hart = 0;
