/* SPDX-License-Identifier: MIT */
#pragma once

/// call after changing executable code in memory
#define instruction_memory_barrier() asm volatile("fence.i")
