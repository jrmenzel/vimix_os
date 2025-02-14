/* SPDX-License-Identifier: MIT */
#pragma once

/// call after changing executable code in memory to flush instruction caches
#define instruction_memory_barrier() asm volatile("fence.i")
