# SPDX-License-Identifier: MIT

#include <arch/riscv/asm/registers.h>
#include <arch/riscv/asm/m_mode.h>
#include <arch/riscv/riscv.h>
#include <kernel/param.h>
#include <kernel/page.h>

.section .text
.global _entry_m_mode
_entry_m_mode:
    # save dtb parameter and hart id,
    # use saved registers; s0 == fp!
    #define hart_id s1
    #define dtb s2
    mv dtb, a1
    csrr hart_id, mhartid
    
    # CPU ID >= MAX_CPU -> infinite loop for you
    li t0, MAX_CPUS
    bge hart_id, t0, infinite_loop
    
    # from here on all supported harts run (hartid <= MAX_CPUS for static arrays)

    # setup stack for M Mode
    la sp, g_m_mode_cpu_stack    # sp = g_m_mode_cpu_stack
    li t0, M_MODE_STACK          # t0 = M_MODE_STACK / stack size
    addi t1, hart_id, 1          # t1 = hartid + 1 (+1 as the stack grows down, so set sp to the "end" of the stack space, not the "begining")
    mul t2, t0, t1               # t2 = ((hartid+1) * M_MODE_STACK)
    add sp, sp, t2               # sp = g_m_mode_cpu_stack + ((hartid+1) * M_MODE_STACK)
    mv fp, zero
    csrw mscratch, sp            # save stack into sratch register

    # jump into C
    mv a0, hart_id
    jal m_mode_start

    # boot hart lottery:
    la t2, g_m_mode_boot_hart
    amoswap.w t1, hart_id, (t2)
    li t2, MAX_CPUS
    bne t1, t2, wait_for_wakeup # lost the lottery
    
    # won the lottery -> this is the boot hart
    mv a0, hart_id # hart id for m_mode_boot_hart_setup()
    mv a1, dtb     # dtb for m_mode_boot_hart_setup()
    jal m_mode_boot_hart_setup
    mv a0, zero
    jal m_mode_prepare_start_hart

    # "returns" to _entry_s_mode in s mode
    mret

wait_for_wakeup:
    wfi
    j wait_for_wakeup
