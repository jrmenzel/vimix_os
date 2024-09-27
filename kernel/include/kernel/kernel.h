/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/types.h>

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

/// First address after the kernel.
/// Defined by kernel.ld.
extern char start_of_kernel[];
extern char end_of_kernel[];

extern char bss_start[];  ///< defined by the linker script
extern char bss_end[];    ///< defined by the linker script

extern char size_of_text[];
extern char size_of_rodata[];
extern char size_of_data[];
extern char size_of_bss[];
