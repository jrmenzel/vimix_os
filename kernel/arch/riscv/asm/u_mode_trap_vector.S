# SPDX-License-Identifier: MIT

#
# low-level code to handle traps from user space into
# the kernel, and returns from kernel to user.
#
# the kernel maps the page holding this code
# at the same virtual address in user and kernel space 
# so that it continues to work when it switches page tables.
# kernel.ld causes this code to start at 
# a page boundary.
#

#include <arch/riscv/asm/registers.h>
#include <arch/riscv/asm/satp.h>
#include <mm/memlayout.h>

#define SAVE_REG_TO_TRAP_FRAME(REGISTER, OFFSET) SAVE_REG_TO_ARRAY(REGISTER, OFFSET, a0)
#define LOAD_REG_FROM_TRAP_FRAME(REGISTER, OFFSET) LOAD_REG_FROM_ARRAY(REGISTER, OFFSET, a0)

# Define a section for this code to place it in the linker script of the kernel in its own 
# (page aligned) page. proc_pagetable() will map only this page into the users page table (so this
# code must set the kernels pagetable before calling kernel functions).
.section trampsec, "ax", @progbits
.globl u_mode_trap_vector
.type u_mode_trap_vector, @function
u_mode_trap_vector:    
    #
    # trap.c sets stvec to point here, so
    # traps from user space start here,
    # in supervisor mode, but with a
    # user page table.
    #

    # switch to kernel stack, store user stack in sscratch till its saved in the trapframe.
    csrrw sp, sscratch, sp

    # save user a0
    addi sp, sp, -1*SIZE_OF_REG
    SAVE_REG_TO_STACK(a0, 0)

    # each process has a separate p->trapframe memory area,
    # but it's mapped to the same virtual address
    # (TRAPFRAME) in every process's user page table.
    li a0, TRAPFRAME
    
    # save the user registers in TRAPFRAME
    SAVE_REG_TO_TRAP_FRAME(ra, IDX_TRAPFRAME_RA)
    SAVE_REG_TO_TRAP_FRAME(gp, IDX_TRAPFRAME_GP)
    SAVE_REG_TO_TRAP_FRAME(tp, IDX_TRAPFRAME_TP)
    SAVE_REG_TO_TRAP_FRAME(t0, IDX_TRAPFRAME_T0)
    SAVE_REG_TO_TRAP_FRAME(t1, IDX_TRAPFRAME_T1)
    SAVE_REG_TO_TRAP_FRAME(t2, IDX_TRAPFRAME_T2)
    SAVE_REG_TO_TRAP_FRAME(s0, IDX_TRAPFRAME_FP)
    SAVE_REG_TO_TRAP_FRAME(s1, IDX_TRAPFRAME_S1)

    # a0 is used to index trapframe, the users
    # value is in sscratch. Luckily t0 is already saved.
    #csrr t0, sscratch
    LOAD_REG_FROM_STACK(t0, 0)
    addi sp, sp, SIZE_OF_REG
    SAVE_REG_TO_TRAP_FRAME(t0, IDX_TRAPFRAME_A0)

    SAVE_REG_TO_TRAP_FRAME(a1, IDX_TRAPFRAME_A1)
    SAVE_REG_TO_TRAP_FRAME(a2, IDX_TRAPFRAME_A2)
    SAVE_REG_TO_TRAP_FRAME(a3, IDX_TRAPFRAME_A3)
    SAVE_REG_TO_TRAP_FRAME(a4, IDX_TRAPFRAME_A4)
    SAVE_REG_TO_TRAP_FRAME(a5, IDX_TRAPFRAME_A5)
    SAVE_REG_TO_TRAP_FRAME(a6, IDX_TRAPFRAME_A6)
    SAVE_REG_TO_TRAP_FRAME(a7, IDX_TRAPFRAME_A7)
    SAVE_REG_TO_TRAP_FRAME(s2, IDX_TRAPFRAME_S2)
    SAVE_REG_TO_TRAP_FRAME(s3, IDX_TRAPFRAME_S3)
    SAVE_REG_TO_TRAP_FRAME(s4, IDX_TRAPFRAME_S4)
    SAVE_REG_TO_TRAP_FRAME(s5, IDX_TRAPFRAME_S5)
    SAVE_REG_TO_TRAP_FRAME(s6, IDX_TRAPFRAME_S6)
    SAVE_REG_TO_TRAP_FRAME(s7, IDX_TRAPFRAME_S7)
    SAVE_REG_TO_TRAP_FRAME(s8, IDX_TRAPFRAME_S8)
    SAVE_REG_TO_TRAP_FRAME(s9, IDX_TRAPFRAME_S9)
    SAVE_REG_TO_TRAP_FRAME(s10, IDX_TRAPFRAME_S10)
    SAVE_REG_TO_TRAP_FRAME(s11, IDX_TRAPFRAME_S11)
    SAVE_REG_TO_TRAP_FRAME(t3, IDX_TRAPFRAME_T3)
    SAVE_REG_TO_TRAP_FRAME(t4, IDX_TRAPFRAME_T4)
    SAVE_REG_TO_TRAP_FRAME(t5, IDX_TRAPFRAME_T5)
    SAVE_REG_TO_TRAP_FRAME(t6, IDX_TRAPFRAME_T6)

    # get user stack pointer from sscratch
    csrr t0, sscratch
    SAVE_REG_TO_TRAP_FRAME(t0, IDX_TRAPFRAME_SP)

    # set stack frame to an invalid value
    mv s0, zero

    # make tp hold the current hartid, from p->trapframe->kernel_hartid
    LOAD_REG_FROM_TRAP_FRAME(tp, IDX_TRAPFRAME_KERNEL_HARTID)

    # load the address of user_mode_interrupt_handler(), from p->trapframe->kernel_trap
    LOAD_REG_FROM_TRAP_FRAME(s1, IDX_TRAPFRAME_KERNEL_TRAP)

    # fetch the kernel page table register value, from p->trapframe->kernel_page_table.
    # mmu_set_kernel_pgtable_reg_value(p->trapframe->kernel_page_table)
    LOAD_REG_FROM_TRAP_FRAME(a0, IDX_TRAPFRAME_KERNEL_PAGE_TABLE)
    mv a1, zero
    jal mmu_set_kernel_pgtable_reg_value

    mv a0, sp

    # jump to user_mode_interrupt_handler(), which does not return
    jr s1

# Sets satp register including all required barriers.
# Implemented in assembly to share code with the other kernel code.
# param a0 = size_t satp register value to set.
.globl mmu_set_kernel_pgtable_reg_value
.type mmu_set_kernel_pgtable_reg_value, @function
mmu_set_kernel_pgtable_reg_value:
    # install the page table
    csrw satp, a0

    # flush the instruction cache now that the code changed by updating the page table
    fence.i

    # flush now-stale entries from the TLB
    sfence.vma zero, zero
    ret

# Sets satp register including all required barriers.
# Implemented in assembly to share code with the other kernel code.
# param a0 = size_t satp register value to set.
# param a1 = ASID
.globl mmu_set_user_pgtable_reg_value
.type mmu_set_user_pgtable_reg_value, @function
mmu_set_user_pgtable_reg_value:
    li t0, SATP_ASID_MASK
    and a1, a1, t0
    li t0, SATP_ASID_POS
    srl a1, a1, t0
    # a1 is now the ASID

    # install the page table
    csrw satp, a0

    # flush the instruction cache now that the code changed by updating the page table
    fence.i

    # flush now-stale entries from the TLB
    sfence.vma zero, a1
    ret

.globl return_to_user_mode_asm
.type return_to_user_mode_asm, @function
return_to_user_mode_asm:
    # return_to_user_mode_asm(pagetable, ASID, stack)
    # called by return_to_user_mode() in trap.c to
    # switch from kernel to user.

    # store kernel stack of process in sscratch, switch stack to user stack
    mv sp, a2
    csrrw sp, sscratch, sp

    # a0: user page table, for satp.
    # a1: ASID
    jal mmu_set_user_pgtable_reg_value

    # restore all but a0 and sp from TRAPFRAME
    li a0, TRAPFRAME

    LOAD_REG_FROM_TRAP_FRAME(ra, IDX_TRAPFRAME_RA)
    LOAD_REG_FROM_TRAP_FRAME(gp, IDX_TRAPFRAME_GP)
    LOAD_REG_FROM_TRAP_FRAME(tp, IDX_TRAPFRAME_TP)
    LOAD_REG_FROM_TRAP_FRAME(t0, IDX_TRAPFRAME_T0)
    LOAD_REG_FROM_TRAP_FRAME(t1, IDX_TRAPFRAME_T1)
    LOAD_REG_FROM_TRAP_FRAME(t2, IDX_TRAPFRAME_T2)
    LOAD_REG_FROM_TRAP_FRAME(s0, IDX_TRAPFRAME_FP)
    LOAD_REG_FROM_TRAP_FRAME(s1, IDX_TRAPFRAME_S1)
    # a0 is loaded later (used to index trapframe)
    LOAD_REG_FROM_TRAP_FRAME(a1, IDX_TRAPFRAME_A1)
    LOAD_REG_FROM_TRAP_FRAME(a2, IDX_TRAPFRAME_A2)
    LOAD_REG_FROM_TRAP_FRAME(a3, IDX_TRAPFRAME_A3)
    LOAD_REG_FROM_TRAP_FRAME(a4, IDX_TRAPFRAME_A4)
    LOAD_REG_FROM_TRAP_FRAME(a5, IDX_TRAPFRAME_A5)
    LOAD_REG_FROM_TRAP_FRAME(a6, IDX_TRAPFRAME_A6)
    LOAD_REG_FROM_TRAP_FRAME(a7, IDX_TRAPFRAME_A7)
    LOAD_REG_FROM_TRAP_FRAME(s2, IDX_TRAPFRAME_S2)
    LOAD_REG_FROM_TRAP_FRAME(s3, IDX_TRAPFRAME_S3)
    LOAD_REG_FROM_TRAP_FRAME(s4, IDX_TRAPFRAME_S4)
    LOAD_REG_FROM_TRAP_FRAME(s5, IDX_TRAPFRAME_S5)
    LOAD_REG_FROM_TRAP_FRAME(s6, IDX_TRAPFRAME_S6)
    LOAD_REG_FROM_TRAP_FRAME(s7, IDX_TRAPFRAME_S7)
    LOAD_REG_FROM_TRAP_FRAME(s8, IDX_TRAPFRAME_S8)
    LOAD_REG_FROM_TRAP_FRAME(s9, IDX_TRAPFRAME_S9)
    LOAD_REG_FROM_TRAP_FRAME(s10,IDX_TRAPFRAME_S10)
    LOAD_REG_FROM_TRAP_FRAME(s11,IDX_TRAPFRAME_S11)
    LOAD_REG_FROM_TRAP_FRAME(t3, IDX_TRAPFRAME_T3)
    LOAD_REG_FROM_TRAP_FRAME(t4, IDX_TRAPFRAME_T4)
    LOAD_REG_FROM_TRAP_FRAME(t5, IDX_TRAPFRAME_T5)
    LOAD_REG_FROM_TRAP_FRAME(t6, IDX_TRAPFRAME_T6)

    # restore user a0
    LOAD_REG_FROM_TRAP_FRAME(a0, IDX_TRAPFRAME_A0)
    
    # return to user mode and user pc.
    # return_to_user_mode() set up sstatus and sepc.
    sret
