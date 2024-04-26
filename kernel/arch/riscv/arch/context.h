/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/riscv.h>
#include <kernel/types.h>

/// Saved registers for kernel context switches.
struct context
{
    uint64_t ra;
    uint64_t sp;

    // callee-saved
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};

void context_switch(struct context*, struct context*);

/// per-process data for the trap handling code in u_mode_trap_vector.S.
/// sits in a page by itself just under the trampoline page in the
/// user page table. not specially mapped in the kernel page table.
/// u_mode_trap_vector in u_mode_trap_vector.S saves user registers in the
/// trapframe, then initializes registers from the trapframe's kernel_sp,
/// kernel_hartid, kernel_page_table, and jumps to kernel_trap.
/// return_to_user_mode() and return_to_user_mode_asm in u_mode_trap_vector.S
/// set up the trapframe's kernel_*, restore user registers from the trapframe,
/// switch to the user page table, and enter user space. the trapframe includes
/// callee-saved user registers like s0-s11 because the return-to-user path via
/// return_to_user_mode() doesn't return through the entire kernel call stack.
struct trapframe
{
    /*   0 */ uint64_t kernel_page_table;  // kernel page table
    /*   8 */ uint64_t kernel_sp;          // top of process's kernel stack
    /*  16 */ uint64_t kernel_trap;        // user_mode_interrupt_handler()
    /*  24 */ uint64_t epc;                // saved user program counter
    /*  32 */ uint64_t kernel_hartid;      // saved kernel tp
    /*  40 */ uint64_t ra;
    /*  48 */ uint64_t sp;
    /*  56 */ uint64_t gp;
    /*  64 */ uint64_t tp;
    /*  72 */ uint64_t t0;
    /*  80 */ uint64_t t1;
    /*  88 */ uint64_t t2;
    /*  96 */ uint64_t s0;
    /* 104 */ uint64_t s1;
    /* 112 */ uint64_t a0;
    /* 120 */ uint64_t a1;
    /* 128 */ uint64_t a2;
    /* 136 */ uint64_t a3;
    /* 144 */ uint64_t a4;
    /* 152 */ uint64_t a5;
    /* 160 */ uint64_t a6;
    /* 168 */ uint64_t a7;
    /* 176 */ uint64_t s2;
    /* 184 */ uint64_t s3;
    /* 192 */ uint64_t s4;
    /* 200 */ uint64_t s5;
    /* 208 */ uint64_t s6;
    /* 216 */ uint64_t s7;
    /* 224 */ uint64_t s8;
    /* 232 */ uint64_t s9;
    /* 240 */ uint64_t s10;
    /* 248 */ uint64_t s11;
    /* 256 */ uint64_t t3;
    /* 264 */ uint64_t t4;
    /* 272 */ uint64_t t5;
    /* 280 */ uint64_t t6;
};
