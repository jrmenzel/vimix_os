/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/param.h>
#include <kernel/printk.h>
#include <kernel/types.h>

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

/// First address after the kernel.
/// Defined by kernel.ld.
extern char end_of_kernel[];
