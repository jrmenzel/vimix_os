/* SPDX-License-Identifier: MIT */
#pragma once

//
// WARNING: This file is included from assembly files,
//          use asm compatible macros only.
//

#include <kernel/page.h>
#include <mm/mm.h>

#if defined(__ARCH_32BIT)
#define HIGHEST_USER_PAGE (0x7FFFF000)
#define KSTACK_VA_END (0xFFFFF000)

#else
#define HIGHEST_USER_PAGE (USER_VA_END - PAGE_SIZE)
#define KSTACK_VA_END (0xFFFFFFFFFFFFF000UL)

#endif

/// map kernel stacks beneath the trampoline,
/// each surrounded by invalid guard pages.
#define KSTACK_VA_FROM_INDEX(idx) \
    (KSTACK_VA_END - ((idx) + 1) * (PAGE_SIZE + KERNEL_STACK_SIZE))

#define KSTACK_INDEX_FROM_VA(va) \
    (((KSTACK_VA_END - (va)) / (PAGE_SIZE + KERNEL_STACK_SIZE)) - 1)

/// User memory layout.
/// Address 400000:
///   text
///   original data and bss
///   expandable heap
///   ...
///   stack
///   TRAPFRAME (p->trapframe, used by the trampoline)
///   TRAMPOLINE (mapped somewhere in the higher half)
#define TRAPFRAME (HIGHEST_USER_PAGE)

/// Highest address of the user stack.
/// Could be placed anywhere. The highest position possible is just below the
/// TRAPFRAME (setting USER_STACK_HIGH to TRAPFRAME - remember the stack grows
/// down!).
#define USER_STACK_HIGH (USER_VA_END - 16 * PAGE_SIZE)
