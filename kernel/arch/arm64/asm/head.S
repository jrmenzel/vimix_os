/* SPDX-License-Identifier: MIT */

// kernel.ld causes the following code to
// be placed at the kernel start location.

#include <asm/registers.h>
#include <arch/arm64/asm/arm64_def.h>
#include <kernel/param.h>

// save some relevant values in registers that will survive calling C code:
#define DTB_REG x19
#define CPU_ID_REG x20
#define PHYS_BASE_REG x23

.section .text
.global _start
_start:
    // Linux style header to get booted by various boot loaders with the same protocol.
    // code0/code1 = 64 bits for code to jump over header
    ccmp x18, #0, #0xd, pl             // generates "MZ", needed by some boot loaders
    b _behind_header                   // jump over header
    .quad TEXT_OFFSET                  // text offset
    .quad __kernel_image_size          // image_size
    .quad 0                            // flags: little-endian
    .quad 0                            // reserved
    .quad 0                            // reserved
    .quad 0                            // reserved
    .ascii "ARM\x64"                   // magic: 0x644d5241
    .long 0                            // PE-COFF offset

_behind_header:
    mov DTB_REG, x0

#if defined(__CONFIG_DTB_FILE_PATH)
    adrp DTB_REG, dtb_embedded         // load the address of the embedded DTB
    add DTB_REG, DTB_REG, :lo12:dtb_embedded
#endif

    mrs CPU_ID_REG, mpidr_el1          // get CPU ID
    and CPU_ID_REG, CPU_ID_REG, #0xff  // mask to get the CPU index within the cluster, assuming only one cluster for now

    cbz CPU_ID_REG, init_core          // only boot CPU 0 (others later)
    b infinite_loop_asm

init_core:
    bl clear_kernel_bss
    bl phys_base_reg_init

    bl setup_el2_if_needed
    // We are now definitely in EL1

    // Always run EL1 with SP_EL1. This avoids taking kernel exceptions on
    // SP_EL0 and selecting the unsupported "Current EL with SP_EL0" vector
    // slots.
    msr spsel, #1
    isb

    // set up exception handlers to catch errors building the early page table
    adrp x2, trap_vectors
    add x2, x2, :lo12:trap_vectors
    msr vbar_el1, x2

    // Ensure the change takes effect
    isb

    // expose logical CPU index to C code (smp_processor_id).
    msr tpidr_el1, CPU_ID_REG
    isb

    bl setup_boot_stack

    // build early page table in C, provide local pointers to global variables
    // to make sure the code is position independent.
    adrp x0, g_early_kernel_page_table
    add  x0, x0, :lo12:g_early_kernel_page_table
    mov  x1, DTB_REG
    adrp x2, g_early_memory_map
    add  x2, x2, :lo12:g_early_memory_map
    mov  x3, PHYS_BASE_REG
    bl early_pgtable_init
    // return value in x0 is the TTBR register value
    adrp x1, g_kernel_pgtable_reg_value
    add  x1, x1, :lo12:g_kernel_pgtable_reg_value
    STORE_SIZET(x0, x1) // store the TTBR value into g_kernel_pgtable_reg_value

    bl enable_pagetable_and_relocate

    // now in higher-half VA: rebuild stack and continue in C
    bl setup_boot_stack

    // keep exception vectors pinned to the final higher-half address
    adrp x2, trap_vectors
    add x2, x2, :lo12:trap_vectors
    msr vbar_el1, x2
    isb

    // main(dtb_virt, is_boot_hart=true)
    mov x0, DTB_REG
    sub x0, x0, PHYS_BASE_REG
    ldr x1, =PAGE_OFFSET
    add x0, x0, x1
    mov x1, #1
    bl main

    // failsave: main() should not return
    b infinite_loop_asm



///////////////////////////////////////////////////////////////////////////////
//
// All secondary harts get started here by the boot hart via PSCI calls
// 
// PSCI context points to struct { dtb_pa, cpu_idx } in physical memory.
.global _start_secondary
_start_secondary:
    mrs CPU_ID_REG, mpidr_el1
    and CPU_ID_REG, CPU_ID_REG, #0xff
    msr tpidr_el1, CPU_ID_REG          // save CPU ID
    isb

    bl setup_el2_if_needed             // secondary CPUs can also boot in EL2
    // We are now definitely in EL1
    
    bl phys_base_reg_init              // also get phys_base_reg for relocation

    // Always run EL1 with SP_EL1. This avoids taking kernel exceptions on
    // SP_EL0 and selecting the unsupported "Current EL with SP_EL0" vector
    // slots.
    msr spsel, #1
    isb

    // Reuse the last installed kernel TTBR value.
    adrp x10, g_kernel_pgtable_reg_value
    add x10, x10, :lo12:g_kernel_pgtable_reg_value
    ldr x0, [x10]
    bl enable_pagetable_and_relocate

    // now in higher-half VA: rebuild stack and continue in C
    bl setup_boot_stack

    // keep exception vectors pinned to the final higher-half address
    adrp x11, trap_vectors
    add x11, x11, :lo12:trap_vectors
    msr vbar_el1, x11
    isb

    // main(dtb_virt=NULL, is_boot_hart=false)
    mov x0, #0
    mov x1, #0
    bl main

    // main() should never return, but just in case:
    b infinite_loop_asm



///////////////////////////////////////////////////////////////////////////////
//
setup_el2_if_needed:
    mov x9, x30 // save return address

    // get current exception level:
    // EL3 = Firmware / Secure Monitor                          -> we don't expect that and panic
    // EL2 = Hypervisor (kernel should boot here)               -> push some magic values into registers and drop to EL1, never to return
    // EL1 = OS (kernel will only use this once jumped to EL1)  -> Prepare for C and jump to main()
    // EL0 = user space
    mrs x10, CurrentEL
    and x10, x10, EL_MASK // clear reserved bits

    // booting from EL3 is unsupported
    cmp x10, EL_3
    beq infinite_loop_asm

    cmp x10, EL_1
    beq 1f
    bl setup_el2_and_jump_to_el1
1:
    mov x30, x9 // restore return address
    ret

///////////////////////////////////////////////////////////////////////////////
//
setup_el2_and_jump_to_el1:
    // enable CNTP / counter and timer access for EL1
    mrs     x10, cnthctl_el2
    orr     x10, x10, #3
    msr     cnthctl_el2, x10
    msr     cntvoff_el2, xzr // counter offset to 0
    // enable AArch64 in EL1
    ldr     x11, =HCR_VALUE
    msr     hcr_el2, x11
    mrs     x11, hcr_el2
    // change execution level to EL1
    ldr     x12, =SPSR_EL2_VALUE
    msr     spsr_el2, x12
    //adr     x12, boot_from_el1
    //msr     elr_el2, x12
    msr     elr_el2, lr
    eret



///////////////////////////////////////////////////////////////////////////////
//
infinite_loop_asm:
    wfe                                // wait for exception/interrupt, can save power
    b infinite_loop_asm



///////////////////////////////////////////////////////////////////////////////
//
// get the location of the start of RAM and store in PHYS_BASE_REG
.global phys_base_reg_init
phys_base_reg_init:
    adr PHYS_BASE_REG, phys_base_reg_init // PHYS_BASE_REG = current PC (nearby label address)
    adr x0, phys_base_reg_init            // x0 = address of label phys_base_reg_init
    adr x1, _start                        // x1 = address of label _start (kernel start)
    sub x0, x0, x1                        // x0 = offset of this code from kernel start
    sub PHYS_BASE_REG, PHYS_BASE_REG, x0  // PHYS_BASE_REG = current PC - offset = kernel start address
    ldr x0, =TEXT_OFFSET                  // x0 = text offset
    sub PHYS_BASE_REG, PHYS_BASE_REG, x0  // PHYS_BASE_REG = kernel start address - text offset = physical RAM base
    ret



///////////////////////////////////////////////////////////////////////////////
//
// Clear BSS section. Alignment is guaranteed by the linker.
clear_kernel_bss:
    adrp x0, __start_bss               // linker provided
    add x0, x0, :lo12:__start_bss      // 
    adrp x1, __end_bss                 // linker provided
    add x1, x1, :lo12:__end_bss        //
    sub x1, x1, x0                     // x1 = size of memory to clear
1:                                     // do {
    STORE_SIZET(xzr, x0)               //   *addr = 0
    add x0, x0, #SIZE_OF_REG           //   addr += sizeof(size_t)
    sub x1, x1, #SIZE_OF_REG           //   size -= sizeof(size_t)
    cbnz x1, 1b                        // } while(size > 0)
    ret



///////////////////////////////////////////////////////////////////////////////
//
// Set up a stack for EL1 / kernel mode.
.global setup_boot_stack
setup_boot_stack:
    // prepare kernel stack
    adrp x2, g_kernel_cpu_stack
    add x2, x2, :lo12:g_kernel_cpu_stack
    ldr x3, =KERNEL_STACK_SIZE
    add x0, CPU_ID_REG, #1             // x0 = CPU ID + 1
                                       // (+1 as the stack grows down: set sp to the stacks "end", not the "start")
    mul x0, x0, x3                     // x0 = (CPU ID + 1) * stack_size
    add sp, x0, x2                     // per CPU stack addr
    mov fp, #0
    ret



///////////////////////////////////////////////////////////////////////////////
//
// x0: TTBR value
enable_pagetable_and_relocate:
    sub x10, lr, PHYS_BASE_REG
    ldr x11, =PAGE_OFFSET
    add x10, x10, x11                  // x10 = new VA of return address

    // configure MMU registers and enable translation
    ldr x12, =MAIR_VALUE
    msr mair_el1, x12
    ldr x12, =TCR_VALUE
    msr tcr_el1, x12
    msr ttbr0_el1, x0
    msr ttbr1_el1, x0
    dsb ish
    isb

    ldr x12, =SCTLR_VALUE_MMU_DISABLED
    orr x12, x12, #SCTLR_MMU_ENABLED
    msr sctlr_el1, x12
    isb

    br x10
