# SPDX-License-Identifier: MIT

# qemu -kernel loads the kernel at 
#   0x80000000 for -bios none
#   0x80200000 for -bios default (OpenSBI)
# and causes each hart (i.e. CPU) to jump there.
# kernel.ld causes the following code to
# be placed at this kernel start location.

#include <kernel/page.h>
#include <kernel/param.h>

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
    # a0 = hartid; already set in SBI mode

    # code0/code1 = 64 bits for code to jump over header
    c.li    s4, -13              # generates "MZ", needed by some boot loaders
#ifdef CONFIG_RISCV_BOOT_M_MODE
    j _entry_m_mode
#else
    j _behind_header
#endif // CONFIG_RISCV_BOOT_M_MODE
    .byte 0, 0, 0, 0             # each jump is only 2 bytes, fill up code0/code1
    .quad 0                      # text offset: 0x200000?
    .quad end_of_kernel - _entry # image_size
    .quad 0                      # flags: little endian
    .long 2                      # version
    .long 0                      # reserved
    .quad 0                      # reserved
    .ascii  "RISCV\x00\x00\x00"  # magic number 1, "RISCV" (8 bytes)
    .ascii  "RSC\x05"            # magic number 2, "RSC\x05" (4 bytes)
    .long 0                      # reserved

_behind_header:
#if defined(__PLATFORM_VISIONFIVE2)
    li a0, 1  # starfive,boot-hart-id in device tree
    #li a1, 0xfffc6080 # in firmware device tree
    li a1, 0x41000000 # set in boot.cmd: dtb was explicitly loaded to that location
#endif
#if defined(__CONFIG_DTB_EMBEDDED)
    la a1, dtb_embedded
#endif
    #li a1, 0x50000000 # device tree loaded by u-boot
.global _entry_s_mode
_entry_s_mode:
    # Thanks to the boot loader (like OpenSBI) or our _entry_m_mode we now have 
    # hartid (CPU ID) in a0 and a pointer to the device tree in a1.
    # keep each CPU's hartid in its tp register, for smp_processor_id().
    mv tp, a0

    # CPU ID >= MAX_CPU -> infinite loop for you
    li t0, MAX_CPUS
    bge a0, t0, infinite_loop

    # set up a stack for C.
    # g_kernel_cpu_stack is declared in start.c with at least one page per CPU
    la sp, g_kernel_cpu_stack    # sp = g_kernel_cpu_stack
    li t0, KERNEL_STACK_SIZE     # t0 = KERNEL_STACK_SIZE, multiple of PAGE_SIZE
    addi t1, a0, 1               # t1 = hartid + 1 (+1 as the stack grows down, so set sp to the "end" of the stack space, not the "begining")
    mul t2, t0, t1               # t2 = ((hartid+1) * KERNEL_STACK_SIZE)
    add sp, sp, t2               # sp = g_kernel_cpu_stack + ((hartid+1) * KERNEL_STACK_SIZE)
    mv fp, zero

    # jump to start() in start.c
    call start

    # start should never return, but also used if there are too many CPUs:
.global infinite_loop
infinite_loop:
    wfi        # wait for interrupt, can save power
    j infinite_loop
