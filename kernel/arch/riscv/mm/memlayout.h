/* SPDX-License-Identifier: MIT */
#pragma once

//
// WARNING: This file is included from assembly files,
//          use asm compatible macros only.
//

#include <kernel/param.h>
#include <mm/mm.h>

/// map the trampoline page to the highest address,
/// in both user and kernel space.
#if (__riscv_xlen == 32)
#define TRAMPOLINE (0xFFFFF000)
#else
#define TRAMPOLINE (MAXVA - PAGE_SIZE)
#endif

/// map kernel stacks beneath the trampoline,
/// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PAGE_SIZE)

/// User memory layout.
/// Address zero first:
///   text
///   original data and bss
///   fixed-size stack
///   expandable heap
///   ...
///   TRAPFRAME (p->trapframe, used by the trampoline)
///   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PAGE_SIZE)

/// Highest address of the user stack.
/// Could be placed anywhere. The highest position possible is just below the
/// TRAPFRAME (setting USER_STACK_HIGH to TRAPFRAME - remember the stack grows
/// down!). But moving it a bit lower gives nicer stack addresses during
/// debugging :-)
#define USER_STACK_HIGH (TRAPFRAME - 13 * PAGE_SIZE)
