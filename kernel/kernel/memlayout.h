/* SPDX-License-Identifier: MIT */
#pragma once

//
// WARNING: This file is included from assembly files,
//          use asm compatible macros only.
//

#include <kernel/page.h>
#include <mm/mm.h>

/// map the trampoline page to the highest address,
/// in both user and kernel space.
#if defined(__ARCH_32BIT)
#define TRAMPOLINE (0xFFFFF000)
#else
#define TRAMPOLINE (USER_VA_END - PAGE_SIZE)
#endif

/// map kernel stacks beneath the trampoline,
/// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * (PAGE_SIZE + KERNEL_STACK_SIZE))

/// User memory layout.
/// Address 400000:
///   text
///   original data and bss
///   expandable heap
///   ...
///   stack
///   TRAPFRAME (p->trapframe, used by the trampoline)
///   TRAMPOLINE (the same page as in the kernel)
// #define TRAPFRAME (USER_VA_END - PAGE_SIZE)
#define TRAPFRAME (TRAMPOLINE - PAGE_SIZE)

/// Highest address of the user stack.
/// Could be placed anywhere. The highest position possible is just below the
/// TRAPFRAME (setting USER_STACK_HIGH to TRAPFRAME - remember the stack grows
/// down!).
#define USER_STACK_HIGH (USER_VA_END - 16 * PAGE_SIZE)
