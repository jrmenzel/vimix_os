# SPDX-License-Identifier: MIT

# qemu -kernel loads the kernel at 
#   0x80000000 for -bios none
#   0x80200000 for -bios default (OpenSBI)
# and causes each hart (i.e. CPU) to jump there.
# kernel.ld causes the following code to
# be placed at this kernel start location.

#include <asm/pgtable-bits.h>
#include <asm/registers.h>
#include <kernel/page.h>
#include <kernel/param.h>
#include <arch/riscv/asm/shared_asm.S>

#
# Macros for kernel relocation
#

# save some relevant values in registers that will survive calling C code:
#define dtb s1
#define PHYS_BASE s2

# start and end must be size_t aligned
# (end-start) must be multiple of size_t and > 0
.macro CLEAR_MEMORY start end
    lla t0, \start
    lla t1, \end
    sub t1, t1, t0
1:
        STORE_SIZET(zero, t0)  # *addr = 0
        addi t0, t0, SIZE_OF_REG
        addi t1, t1, -SIZE_OF_REG
        bnez t1, 1b
2:
.endm

# expects the satp register value in a0
.macro ENABLE_PAGETABLE_AND_RELOCATE
    lla   t0, 1f
    sub   t0, t0, PHYS_BASE
    li    t1, PAGE_OFFSET
    add   t0, t0, t1            # t0 = new VA of label 1

    csrw  satp, a0              # enable paging
    sfence.vma zero, zero       # flush TLB
    fence.i                     # instruction fence
    jalr  zero, t0, 0           # jump to label in new VA space

1:
.endm

#
# When run in S-Mode (on top of OpenSBI), the boot loader jumps here with one hart (CPU).
# All other harts wait to be started via SBI calls. VIMIX will start those
# not from here but from _entry_s_mode below.
#
# When run in M-Mode all harts start here.
#
.section .text
.global _entry
_entry:
    # a0 = hartid; already set in SBI mode

    # code0/code1 = 64 bits for code to jump over header
    c.li    s4, -13              # generates "MZ", needed by some boot loaders
    j _behind_header
    .byte 0, 0, 0, 0             # each jump is only 2 bytes, fill up code0/code1
    .quad TEXT_OFFSET            # text offset
    .quad __end_of_kernel - __start_kernel # image_size
    .quad 0                      # flags: little endian
    .long 2                      # version
    .long 0                      # reserved
    .quad 0                      # reserved
    .ascii  "RISCV\x00\x00\x00"  # magic number 1, "RISCV" (8 bytes)
    .ascii  "RSC\x05"            # magic number 2, "RSC\x05" (4 bytes)
    .long 0                      # reserved

_behind_header:
    jal _entry_setup_hartid_dtb

    # store hart ID in tp for smp_processor_id() to work
    mv tp, a0
    mv dtb, a1 # store dtb in a saved register

    # CPU ID >= MAX_CPU -> infinite loop for you
    # (if this is the boot hart in S-Mode, boot will fail)
    li t0, MAX_CPUS
    bge tp, t0, infinite_loop

#ifdef CONFIG_RISCV_BOOT_M_MODE
    # M-Mode runs with all cores, perform a boot lottery:
    PERFORM_BOOT_LOTTERY g_early_pt_hart, wait_for_early_page_table
#endif

    # from now on only one hart runs:

    # start with clearing BSS
    CLEAR_MEMORY __start_bss, __end_bss

    # prepare for C
    CREATE_STACK g_kernel_cpu_stack, KERNEL_STACK_SIZE

    # get phys_base
.global phys_base_init
phys_base_init:
    auipc   PHYS_BASE, 0
    lla     t0, phys_base_init
    lla     t1, _entry
    sub     t0, t0, t1
    sub     PHYS_BASE, PHYS_BASE, t0
    li      t0, TEXT_OFFSET
    sub     PHYS_BASE, PHYS_BASE, t0

    # build early page table in C
    lla a0, g_early_kernel_page_table
    mv a1, dtb
    lla a2, g_early_memory_map
    mv a3, PHYS_BASE
    jal early_pgtable_init
    # return value is in a0 = satp register value
    # update dtb pointer: dtb = dtb - PHYS_BASE + PAGE_OFFSET
    sub dtb, dtb, PHYS_BASE
    li t0, PAGE_OFFSET
    add dtb, dtb, t0

#ifdef CONFIG_RISCV_BOOT_M_MODE
    # signal that the page table is ready
    lla t0, g_early_pt_done
    fence rw, w
    sw zero, 0(t0)

    ENABLE_PAGETABLE_AND_RELOCATE
    j all_harts_mmode
#else

    ENABLE_PAGETABLE_AND_RELOCATE
    j _entry_s_mode_boot_hart
#endif

#ifdef CONFIG_RISCV_BOOT_M_MODE
wait_for_early_page_table:
    # wait until the page table is ready
    lla t0, g_early_pt_done
1:
    lw t1, 0(t0)
    bnez t1, 1b

    # finished waiting:
    fence r, rw
    ENABLE_PAGETABLE_AND_RELOCATE

all_harts_mmode:
    j _entry_m_mode # detour before calling _entry_s_mode_boot_hart in S Mode
#endif

.global _entry_s_mode_boot_hart
_entry_s_mode_boot_hart:
    # The boot hart runs this in S-Mode

    # set up a stack for C.
    # g_kernel_cpu_stack is declared in start.c with at least one page per CPU
    CREATE_STACK g_kernel_cpu_stack, KERNEL_STACK_SIZE

    # jump to start() in start.c
    mv a0, dtb // parameter 1: dtb
    li a1, 1 // parameter 2: boot hart bool
    call main

.global _entry_s_mode
_entry_s_mode:
    # All secondary harts get started here by the boot hart via SBI calls
    # store hart ID in tp for smp_processor_id() to work
    mv tp, a0
    // dtb is set by SBI to a1 -> save
    mv dtb, a1

    lla t0, g_kernel_pagetable_register_value
    LOAD_SIZET(a0, t0) # load the address of the kernel page table into a0
    ENABLE_PAGETABLE_AND_RELOCATE

    # set up a stack for C.
    # g_kernel_cpu_stack is declared in start.c with at least one page per CPU
    CREATE_STACK g_kernel_cpu_stack, KERNEL_STACK_SIZE
    
    mv a0, dtb // parameter 1: dtb
    mv a1, zero // parameter 2: boot hart bool
    call main

    # start should never return, but also used if there are too many CPUs:
.global infinite_loop
infinite_loop:
    wfi        # wait for interrupt, can save power
    j infinite_loop


# After calling this, the hart ID is in a0 and the dtb pointer is in a1, for both M and S mode entry.
.global _entry_setup_hartid_dtb
_entry_setup_hartid_dtb:
#ifdef CONFIG_RISCV_BOOT_M_MODE
    csrr a0, mhartid
    // dtb is already set in a1
#else
#if defined(__PLATFORM_VISIONFIVE2)
    li a0, 1  # starfive,boot-hart-id in device tree
    #li a1, 0xfffc6080 # in firmware device tree
    li a1, 0x41000000 # set in boot.cmd: dtb was explicitly loaded to that location
#endif // vision five2
#endif // m mode or s mode
#if defined(__CONFIG_DTB_EMBEDDED)
    lla a1, dtb_embedded
#endif
    ret

