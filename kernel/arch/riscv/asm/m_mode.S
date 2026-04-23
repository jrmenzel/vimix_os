# SPDX-License-Identifier: MIT

#include <arch/riscv/asm/registers.h>
#include <arch/riscv/asm/m_mode.h>
#include <arch/riscv/asm/shared_asm.S>
#include <arch/riscv/riscv.h>
#include <kernel/param.h>
#include <kernel/page.h>

.section .text
.global _entry_m_mode
_entry_m_mode:
    #define dtb s1

    # setup stack for M Mode
    CREATE_STACK g_m_mode_cpu_stack, M_MODE_STACK_SIZE
    csrw mscratch, sp            # save stack into sratch register

    # jump into C for all harts
    mv a0, tp // parameter 1: size_t hartid
    jal m_mode_start

    # boot hart lottery:
    PERFORM_BOOT_LOTTERY g_m_mode_boot_hart, wait_for_wakeup
    
    # won the lottery -> this is the boot hart
    mv a0, tp    # hart id for m_mode_boot_hart_setup()
    mv a1, dtb   # dtb for m_mode_boot_hart_setup()
    jal m_mode_boot_hart_setup
    mv a0, zero
    jal m_mode_prepare_start_hart

    # "returns" to _entry_s_mode_boot_hart in s mode
    mret

wait_for_wakeup:
    wfi
    j wait_for_wakeup
