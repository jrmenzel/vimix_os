/* SPDX-License-Identifier: MIT */

// kernel.ld causes the following code to
// be placed at the kernel's start location.

#include <asm/registers.h>
#include <kernel/param.h>
#include <arch/riscv/asm/shared_asm.S>

// save some relevant values in registers that will survive calling C code:
#define DTB_REG s1
#define PHYS_BASE_REG s2


///////////////////////////////////////////////////////////////////////////////
//
// When run in S-Mode (on top of OpenSBI), the boot loader jumps here with one hart (CPU).
// All other harts wait to be started via SBI calls. VIMIX will start those
// not from here but from _start_secondary below.
//
// When run in M-Mode all harts start here.
//
.section .text
.global _start
_start:
    // Linux style header to get booted by various boot loaders with the same protocol.
    // code0/code1 = 64 bits for code to jump over header
    c.li    s4, -13                    // generates "MZ", needed by some boot loaders
    j _behind_header                   // jump over header
    .byte 0, 0, 0, 0                   // each jump is only 2 bytes, fill up code0/code1
    .quad TEXT_OFFSET                  // text offset
    .quad __kernel_image_size          // image_size
    .quad 0                            // flags: little endian
    .long 2                            // version
    .long 0                            // reserved
    .quad 0                            // reserved
    .ascii  "RISCV\x00\x00\x00"        // magic number 1, "RISCV" (8 bytes)
    .ascii  "RSC\x05"                  // magic number 2, "RSC\x05" (4 bytes)
    .long 0                            // reserved

_behind_header:

#ifdef CONFIG_RISCV_BOOT_M_MODE
    csrw mie, zero                     // disable all interrupts
	csrw mip, zero                     // clear any pending interrupts

    csrr a0, mhartid                   // a0 = hartid; is already set in SBI mode
#else
    csrw sie, zero                     // disable all interrupts
	csrw sip, zero                     // clear any pending interrupts
#endif // m mode or s mode

#if defined(__CONFIG_DTB_REG_FILE_PATH)
    lla a1, dtb_reg_embedded           // load the address of the embedded DTB
#endif

    mv tp, a0                          // store hart ID in tp for smp_processor_id() to work
    mv DTB_REG, a1                     // store device tree pointer into a saved register

    // CPU ID >= MAX_CPU -> infinite loop for you
    // (if this is the boot hart in S-Mode, boot will fail)
    li t0, MAX_CPUS
    bge tp, t0, infinite_loop_asm

    jal phys_base_reg_init             // get phys_base_reg for relocation

#ifdef CONFIG_RISCV_BOOT_M_MODE
    // M-Mode runs with all cores, perform a boot lottery:
    PERFORM_BOOT_LOTTERY g_early_pt_hart, wait_for_early_page_table
#endif

    // from now on only one hart runs:
    jal clear_kernel_bss               // can't rely on the boot loader to have cleared the BSS
    jal setup_boot_stack               // prepare for stack for calling C

    // build early page table in C, provide local pointers to global variables
    // to make sure the code is position independent.
    lla a0, g_early_kernel_page_table
    mv  a1, DTB_REG
    lla a2, g_early_memory_map
    mv  a3, PHYS_BASE_REG
    jal early_pgtable_init
    // return value is in a0 = satp register value
    lla t0, g_kernel_pgtable_reg_value
    STORE_SIZET(a0, t0) // store the satp value into g_kernel_pgtable_reg_value

    // update DTB_REG pointer: DTB_REG = DTB_REG - PHYS_BASE_REG + PAGE_OFFSET
    sub DTB_REG, DTB_REG, PHYS_BASE_REG
    li t0, PAGE_OFFSET
    add DTB_REG, DTB_REG, t0

#ifdef CONFIG_RISCV_BOOT_M_MODE
    // signal that the page table is ready
    lla t0, g_early_pt_done
    fence rw, w
    sw zero, 0(t0)

    jal enable_pagetable_and_relocate
    j _entry_m_mode
#else

    jal enable_pagetable_and_relocate
    j _entry_s_mode_boot_hart
#endif

.global _entry_s_mode_boot_hart
_entry_s_mode_boot_hart:
    // The boot hart runs this in S-Mode

    // set up a stack for C after relocation again because the addesses have changed:
    jal setup_boot_stack

    mv a0, DTB_REG                     // parameter 1: DTB_REG
    li a1, 1                           // parameter 2: boot hart == true
    call main                          // jump to main()



///////////////////////////////////////////////////////////////////////////////
//
// All secondary harts get started here by the boot hart via SBI calls
// 
// a0: hart ID, store in tp for smp_processor_id() to work
.global _start_secondary
_start_secondary:
    mv tp, a0                          // save CPU ID

    jal phys_base_reg_init             // also get phys_base_reg for relocation

    // Reuse the last installed kernel satp value.
    lla t0, g_kernel_pgtable_reg_value
    LOAD_SIZET(a0, t0)
    jal enable_pagetable_and_relocate

    jal setup_boot_stack               // set up a stack for C after relocation
    
    // main(dtb_virt=NULL, is_boot_hart=false)
    mv a0, zero
    mv a1, zero
    call main
    
    // main() should never return, but just in case:
    j infinite_loop_asm



///////////////////////////////////////////////////////////////////////////////
//
infinite_loop_asm:
    wfi                                // wait for interrupt, can save power
    j infinite_loop_asm



///////////////////////////////////////////////////////////////////////////////
//
// get the location of the start of RAM and store in PHYS_BASE_REG
.global phys_base_reg_init
phys_base_reg_init:
    auipc   PHYS_BASE_REG, 0                 // PHYS_BASE_REG = current PC
    lla     t0, phys_base_reg_init           // t0 = address of label phys_base_reg_init
    lla     t1, _start                       // t1 = address of label _start (kernel start)
    sub     t0, t0, t1                       // t0 = offset of this code from the kernel start
    sub     PHYS_BASE_REG, PHYS_BASE_REG, t0 // PHYS_BASE_REG = current PC - offset = kernel start address
    li      t0, TEXT_OFFSET                  // t0 = text offset
    sub     PHYS_BASE_REG, PHYS_BASE_REG, t0 // PHYS_BASE_REG = kernel start address - text offset = physical RAM base
    ret



///////////////////////////////////////////////////////////////////////////////
//
// Clear BSS section. Alignment is guaranteed by the linker.
clear_kernel_bss:
    lla a0, __start_bss                // linker provided
    lla a1, __end_bss                  // linker provided
    sub a1, a1, a0                     // a1 = size of memory to clear
1:                                     // do {
    STORE_SIZET(zero, a0)              //   *addr = 0
    addi a0, a0, SIZE_OF_REG           //   addr += sizeof(size_t)
    addi a1, a1, -SIZE_OF_REG          //   size -= sizeof(size_t)
    bnez a1, 1b                        // } while(size > 0)
    ret



///////////////////////////////////////////////////////////////////////////////
//
// Set up a stack for S-Mode.
.global setup_boot_stack
setup_boot_stack:
    lla a0, g_kernel_cpu_stack
    li a1, KERNEL_STACK_SIZE
    // fall through to entry point setup_per_hart_stack which is used by M-Mode but with
    // different parameters.

// a0: stack array start address
// a1: stack size per CPU
.global setup_per_hart_stack
setup_per_hart_stack:
    addi t1, tp, 1                     // t1 = hartid + 1 
                                       // (+1 as the stack grows down: set sp to the stacks "end", not the "start")
    mul t2, a1, t1                     // t2 = ((hartid+1) * stack_size)
    add sp, a0, t2                     // sp = stack_array + ((hartid+1) * stack_size)
    mv fp, zero                        // clear frame pointer
    ret



///////////////////////////////////////////////////////////////////////////////
//
// a0: satp value
enable_pagetable_and_relocate:
    sub   t0, ra, PHYS_BASE_REG
    li    t1, PAGE_OFFSET
    add   t0, t0, t1                   // t0 = new VA of return address

    csrw  satp, a0                     // enable paging
    sfence.vma zero, zero              // flush TLB
    fence.i                            // instruction fence
    jalr  zero, t0, 0                  // return in new VA space



///////////////////////////////////////////////////////////////////////////////
//
// In M-Mode all CPUs start the kernel, so one is picked to do the page table init,
// all others wait here for the signal and then all CPUs continue in M-Mode.
// Like with a real SBI implementation, one will come back to S-Mode while the others wait.
#ifdef CONFIG_RISCV_BOOT_M_MODE
wait_for_early_page_table:
    // wait until the page table is ready:
    lla t0, g_early_pt_done
1:
    lw t1, 0(t0)
    bnez t1, 1b

    // finished waiting:
    fence r, rw
    
    lla t0, g_kernel_pgtable_reg_value
    LOAD_SIZET(a0, t0)                 // load the kernel's page tables satp value into a0
    jal enable_pagetable_and_relocate

    // ignore the stack, M-Mode sets its own.
    
    j _entry_m_mode                    // jump to the M-Mode entry point
#endif
