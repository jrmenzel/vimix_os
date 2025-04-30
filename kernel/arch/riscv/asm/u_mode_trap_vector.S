# SPDX-License-Identifier: MIT

#
# low-level code to handle traps from user space into
# the kernel, and returns from kernel to user.
#
# the kernel maps the page holding this code
# at the same virtual address (TRAMPOLINE)
# in user and kernel space so that it continues
# to work when it switches page tables.
# kernel.ld causes this code to start at 
# a page boundary.
#

#include <arch/riscv/asm/registers.h>
#include <kernel/memlayout.h>
#include <arch/riscv/asm/satp.h>

# Define a section for this code to place it in the linker script at the end of the kernel,
# page aligned. proc_pagetable() will map only this page into the users page table (so this
# code must set the kernels pagetable before calling functions.
.section trampsec
.globl trampoline
trampoline:
.align 4
.globl u_mode_trap_vector

#define SAVE_REG_TO_TRAP_FRAME(REGISTER, OFFSET) SAVE_REG_TO_ARRAY(REGISTER, OFFSET, a0)
#define LOAD_REG_FROM_TRAP_FRAME(REGISTER, OFFSET) LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, a0)

u_mode_trap_vector:    
    #
    # trap.c sets stvec to point here, so
    # traps from user space start here,
    # in supervisor mode, but with a
    # user page table.
    #

    # save user a0 in sscratch so
    # a0 can be used to get at TRAPFRAME.
    csrw sscratch, a0

    # each process has a separate p->trapframe memory area,
    # but it's mapped to the same virtual address
    # (TRAPFRAME) in every process's user page table.
    li a0, TRAPFRAME
    
    # save the user registers in TRAPFRAME
    SAVE_REG_TO_TRAP_FRAME(ra,  5)
    SAVE_REG_TO_TRAP_FRAME(sp,  6)
    SAVE_REG_TO_TRAP_FRAME(gp,  7)
    SAVE_REG_TO_TRAP_FRAME(tp,  8)
    SAVE_REG_TO_TRAP_FRAME(t0,  9)
    SAVE_REG_TO_TRAP_FRAME(t1, 10)
    SAVE_REG_TO_TRAP_FRAME(t2, 11)
    SAVE_REG_TO_TRAP_FRAME(s0, 12)
    SAVE_REG_TO_TRAP_FRAME(s1, 13)

    # a0 (index 14) is used to index trapframe, the users
    # value is in sscratch. Luckily t0 is already saved.
    csrr t0, sscratch
    SAVE_REG_TO_TRAP_FRAME(t0, 14)

    SAVE_REG_TO_TRAP_FRAME(a1, 15)
    SAVE_REG_TO_TRAP_FRAME(a2, 16)
    SAVE_REG_TO_TRAP_FRAME(a3, 17)
    SAVE_REG_TO_TRAP_FRAME(a4, 18)
    SAVE_REG_TO_TRAP_FRAME(a5, 19)
    SAVE_REG_TO_TRAP_FRAME(a6, 20)
    SAVE_REG_TO_TRAP_FRAME(a7, 21)
    SAVE_REG_TO_TRAP_FRAME(s2, 22)
    SAVE_REG_TO_TRAP_FRAME(s3, 23)
    SAVE_REG_TO_TRAP_FRAME(s4, 24)
    SAVE_REG_TO_TRAP_FRAME(s5, 25)
    SAVE_REG_TO_TRAP_FRAME(s6, 26)
    SAVE_REG_TO_TRAP_FRAME(s7, 27)
    SAVE_REG_TO_TRAP_FRAME(s8, 28)
    SAVE_REG_TO_TRAP_FRAME(s9, 29)
    SAVE_REG_TO_TRAP_FRAME(s10,30)
    SAVE_REG_TO_TRAP_FRAME(s11,31)
    SAVE_REG_TO_TRAP_FRAME(t3, 32)
    SAVE_REG_TO_TRAP_FRAME(t4, 33)
    SAVE_REG_TO_TRAP_FRAME(t5, 34)
    SAVE_REG_TO_TRAP_FRAME(t6, 35)

    # initialize kernel stack pointer, from p->trapframe->kernel_sp
    LOAD_REG_FROM_TRAP_FRAME(sp, 1)

    # set stack frame to an invalid value
    mv s0, zero

    # make tp hold the current hartid, from p->trapframe->kernel_hartid
    LOAD_REG_FROM_TRAP_FRAME(tp, 4)

    # load the address of user_mode_interrupt_handler(), from p->trapframe->kernel_trap
    LOAD_REG_FROM_TRAP_FRAME(s0, 2)

    # fetch the kernel page table register value, from p->trapframe->kernel_page_table.
    # mmu_set_page_table_reg_value(p->trapframe->kernel_page_table)
    LOAD_REG_FROM_TRAP_FRAME(a0, 0)
    jal mmu_set_page_table_reg_value

    # jump to user_mode_interrupt_handler(), which does not return
    jr s0

# Sets satp register including all required barriers.
# Implemented in assembly to share code with the other kernel code.
# param a0 = size_t satp register value to set.
.globl mmu_set_page_table_reg_value
mmu_set_page_table_reg_value:
    li t0, SATP_ASID_MASK
    and a1, a1, t0
    li t0, SATP_ASID_POS
    srl a1, a1, t0
    # a1 is now the ASID

    # redundant?
    sfence.vma zero, a1

    # install the page table
    csrw satp, a0

    # flush the instruction cache now that the code changed by updating the page table
    fence.i

    # flush now-stale entries from the TLB
    sfence.vma zero, a1

    ret

.globl return_to_user_mode_asm
return_to_user_mode_asm:
    # return_to_user_mode_asm(pagetable)
    # called by return_to_user_mode() in trap.c to
    # switch from kernel to user.
    # a0: user page table, for satp.
    jal mmu_set_page_table_reg_value

    li a0, TRAPFRAME

    # restore all but a0 from TRAPFRAME
    
    LOAD_REG_FROM_TRAP_FRAME(ra,  5)
    LOAD_REG_FROM_TRAP_FRAME(sp,  6)
    LOAD_REG_FROM_TRAP_FRAME(gp,  7)
    LOAD_REG_FROM_TRAP_FRAME(tp,  8)
    LOAD_REG_FROM_TRAP_FRAME(t0,  9)
    LOAD_REG_FROM_TRAP_FRAME(t1, 10)
    LOAD_REG_FROM_TRAP_FRAME(t2, 11)
    LOAD_REG_FROM_TRAP_FRAME(s0, 12)
    LOAD_REG_FROM_TRAP_FRAME(s1, 13)
    # a0 (index 14) is loaded later (used to index trapframe)
    LOAD_REG_FROM_TRAP_FRAME(a1, 15)
    LOAD_REG_FROM_TRAP_FRAME(a2, 16)
    LOAD_REG_FROM_TRAP_FRAME(a3, 17)
    LOAD_REG_FROM_TRAP_FRAME(a4, 18)
    LOAD_REG_FROM_TRAP_FRAME(a5, 19)
    LOAD_REG_FROM_TRAP_FRAME(a6, 20)
    LOAD_REG_FROM_TRAP_FRAME(a7, 21)
    LOAD_REG_FROM_TRAP_FRAME(s2, 22)
    LOAD_REG_FROM_TRAP_FRAME(s3, 23)
    LOAD_REG_FROM_TRAP_FRAME(s4, 24)
    LOAD_REG_FROM_TRAP_FRAME(s5, 25)
    LOAD_REG_FROM_TRAP_FRAME(s6, 26)
    LOAD_REG_FROM_TRAP_FRAME(s7, 27)
    LOAD_REG_FROM_TRAP_FRAME(s8, 28)
    LOAD_REG_FROM_TRAP_FRAME(s9, 29)
    LOAD_REG_FROM_TRAP_FRAME(s10,30)
    LOAD_REG_FROM_TRAP_FRAME(s11,31)
    LOAD_REG_FROM_TRAP_FRAME(t3, 32)
    LOAD_REG_FROM_TRAP_FRAME(t4, 33)
    LOAD_REG_FROM_TRAP_FRAME(t5, 34)
    LOAD_REG_FROM_TRAP_FRAME(t6, 35)

    # restore user a0
    LOAD_REG_FROM_TRAP_FRAME(a0, 14)
    
    # return to user mode and user pc.
    # return_to_user_mode() set up sstatus and sepc.
    sret
