# SPDX-License-Identifier: MIT

# qemu -kernel loads the kernel at 
#   0x80000000 for -bios none
#   0x80200000 for -bios default (OpenSBI)
# and causes each hart (i.e. CPU) to jump there.
# kernel.ld causes the following code to
# be placed at this kernel start location.

#include <kernel/param.h>
#include <arch/riscv/mm/mm.h>

#
# All harts (CPU threads) start here:
# * if the OS runs bare metal then in Machine Mode
# * if the OS runs on e.g. OpenSBI then in Supervisor Mode
# This function sets up a stack for each hart and then
# jumps to start() to continue in C. 
#
.section .text
.global _entry
_entry:
    # NOTE: Some boot loader (like OpenSBI) provide hartid (CPU ID)
    #       in a0 and a pointer to the device tree in a1.
    #       Don't touch a1 and main() gets it as the second parameter.
    #       a0 will be set to the hartid if not set already.

    # a0 = hartid; already set in SBI mode
#ifndef CONFIG_RISCV_SBI
    csrr a0, mhartid     # only works in Machine Mode
#endif // CONFIG_RISCV_SBI
#ifdef __PLATFORM_VISIONFIVE2
    # cpu id and device tree is not set as expected.
    # Fix for boot hart only (g_global_init_done == 1)
    la t0, g_global_init_done
    ld t1, 0(t0)
    li t2, 1
    bne t1, t2, 1f
        li a0, 1  # starfive,boot-hart-id in device tree
        li a1, 0xfffc6080 # device tree
1:
#endif
    # keep each CPU's hartid in its tp register, for smp_processor_id().
    mv tp, a0

    # CPU ID >= MAX_CPU -> infinite loop for you
    li t0, MAX_CPUS
    bge a0, t0, infinite_loop

    # set up a stack for C.
    # g_kernel_cpu_stack is declared in start.c with one page per CPU
    la sp, g_kernel_cpu_stack    # sp = g_kernel_cpu_stack
    li t0, KERNEL_STACK_SIZE     # t0 = KERNEL_STACK_SIZE, multiple of PAGE_SIZE
    addi t1, a0, 1               # t1 = hartid + 1 (+1 as the stack grows down, so set sp to the "end" of the stack space, not the "begining")
    mul t2, t0, t1               # t2 = ((hartid+1) * KERNEL_STACK_SIZE)
    add sp, sp, t2               # sp = g_kernel_cpu_stack + ((hartid+1) * KERNEL_STACK_SIZE)
    mv fp, zero

    # jump to start() in start.c
    call start

    # start should never return, but also used if there are too many CPUs:
infinite_loop:
    wfi        # wait for interrupt, can save power
    j infinite_loop
