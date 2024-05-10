/* SPDX-License-Identifier: MIT */
#pragma once

#include <mm/mm.h>

// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtio disk
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end_of_kernel -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

/// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 10

/// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

/// one page where writing to the first int can trigger a qemu shutdown or
/// reboot
#define VIRT_TEST 0x100000

/// written to location VIRT_TEST qemu should shutdown
#define VIRT_TEST_SHUTDOWN 0x5555

/// written to location VIRT_TEST qemu should reboot the emulator
#define VIRT_TEST_REBOOT 0x7777

/// "Goldfish" real-time clock (supported on qemu)
#define RTC_GOLDFISH 0x101000

/// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L

/// Compare value for the timer. Always 64 bit!
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8 * (hartid))

// Cycles since boot. This register is always 64 bit!
#define CLINT_MTIME (CLINT + 0xBFF8)

/// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

/// the kernel expects there to be RAM
/// for use by the kernel and user pages
/// from physical address 0x80000000/0x80200000L to PHYSTOP.
#ifdef MEMORY_SIZE
// set in Makefile
#define RAM_SIZE_IN_MB MEMORY_SIZE
#else
// fallback
#define RAM_SIZE_IN_MB 128
#endif

#ifdef __ENABLE_SBI__
#define KERNBASE 0x80200000L
#define PHYSTOP (KERNBASE + (RAM_SIZE_IN_MB - 2) * 1024 * 1024)
#else
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + RAM_SIZE_IN_MB * 1024 * 1024)
#endif  // __ENABLE_SBI__

/// map the trampoline page to the highest address,
/// in both user and kernel space.
#if (__riscv_xlen == 32)
#define TRAMPOLINE (0xFFFF000)
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
