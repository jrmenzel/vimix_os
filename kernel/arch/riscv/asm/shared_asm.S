/* SPDX-License-Identifier: MIT */

#include <asm/satp.h>

// This file contains macros to be used by the S-Mode and M-Mode entry code


// Given a flag_var of int32 and initialized with 1,
// all harts will exchange the current value with 0 and get the
// old value. Only the first hart will get 1 and continue.
// All others will jump to \looser_label.
.macro PERFORM_BOOT_LOTTERY flag_var, looser_label
    // only one hart should do it, others wait.
    lla t0, \flag_var
    amoswap.w t1, zero, (t0)    // flag_var = 0, 1 returned for winner, 0 for others
    beq t1, zero, \looser_label // lost the lottery
.endm
