# SPDX-License-Identifier: MIT

#include <asm/satp.h>

# This file contains macros to be used by the S-Mode and M-Mode entry code

# sets up a stack, assumes the CPU ID is in tp.
# also clears the frame pointer
.macro CREATE_STACK stack_array, stack_size
    li t0, \stack_size      # t0 = stack size
    addi t1, tp, 1          # t1 = hartid + 1 (+1 as the stack grows down, so set sp to the "end" of the stack space, not the "begining")
    mul t2, t0, t1          # t2 = ((hartid+1) * stack_size)
    lla sp, \stack_array    # sp = g_m_mode_cpu_stack
    add sp, sp, t2          # sp = g_m_mode_cpu_stack + ((hartid+1) * stack_size)
    mv fp, zero
.endm

# Given a flag_var of int32 and initialized with 1,
# all harts will exchange the current value with 0 and get the
# old value. Only the first hart will get 1 and continue.
# All others will jump to \looser_label.
.macro PERFORM_BOOT_LOTTERY flag_var, looser_label
    # only one hart should do it, others wait.
    lla t0, \flag_var
    amoswap.w t1, zero, (t0) # g_early_pt_hart = 0, 1 returned for winner, 0 for others
    beq t1, zero, \looser_label # lost the lottery
.endm
