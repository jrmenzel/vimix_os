# SPDX-License-Identifier: MIT

# qemu -kernel loads the kernel at 0x80000000
# and causes each hart (i.e. CPU) to jump there.
# kernel.ld causes the following code to
# be placed at 0x80000000.

#include <kernel/param.h>
#include <arch/riscv/mm/mm.h>

#
# All harts (CPU threads) start here in Machine Mode.
# This function sets up a stack for each hart and then
# jumps to start() to continue in C. 
#
.section .text
.global _entry
_entry:
    csrr a0, mhartid     # only works in Machine Mode

    # CPU ID >= MAX_CPU -> infinite loop for you
    li t0, MAX_CPUS
    bge a0, t0, infinite_loop

    # set up a stack for C.
    # g_kernel_cpu_stack is declared in start.c with one page per CPU
    la sp, g_kernel_cpu_stack    # sp = g_kernel_cpu_stack
    li t0, PAGE_SIZE             # t0 = PAGE_SIZE, normally 4k
    addi t1, a0, 1               # t1 = hartid + 1 (+1 as the stack grows down, so set sp to the "end" of the stack space, not the "begining")
    mul t2, t0, t1               # t2 = ((hartid+1) * PAGE_SIZE)
    add sp, sp, t2               # sp = g_kernel_cpu_stack + ((hartid+1) * PAGE_SIZE)

    # jump to start() in start.c
    call start

    # start should never return, but also used if there are too many CPUs:
infinite_loop:
    wfi        # wait for interrupt, can save power
    j infinite_loop
