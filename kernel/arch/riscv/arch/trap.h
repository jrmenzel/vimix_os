/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>
#include <kernel/spinlock.h>

/// @brief Set kernel mode trap vector
void set_supervisor_trap_vector();

// On RISC V, the trap vector code needs to be mapped in the user page table so
// that traps can execute (initially) on the users pagetable.
#define MAP_TRAP_VECTOR_TO_USER_PT (1)
