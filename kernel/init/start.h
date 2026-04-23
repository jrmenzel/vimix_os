/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// all values above 0 so the initialized global variable will
// *not* be in BSS (cleared by the kernel after reading this var)
#define KERNEL_INIT_EARLY 1
#define KERNEL_INIT_RELOCATED 2
#define KERNEL_INIT_KMALLOC_READY 3
#define KERNEL_INIT_FULLY_BOOTED 4

/// Tracks state of early boot:
extern volatile size_t g_kernel_init_status;

/// CPU ID of the booting hart
extern size_t g_boot_hart;

//
// to implement per ARCH:
//

void cpu_set_boot_state();

extern char trampoline[];  // u_mode_trap_vector.S

//
// Some const values about the kernel binary provided by the linker.
//

extern char __start_kernel[];
extern char __end_text[];
extern char __start_data[];
extern char __start_rodata[];
extern char __start_bss[];
extern char __end_bss[];
extern char __end_of_kernel[];

extern char __size_of_text[];
extern char __size_of_rodata[];
extern char __size_of_data[];
extern char __size_of_bss[];