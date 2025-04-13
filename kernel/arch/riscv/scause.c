/* SPDX-License-Identifier: MIT */

#include "scause.h"

#include <arch/cpu.h>
#include <kernel/kernel.h>

const char *scause_exception_code_to_string(xlen_t scause)
{
    switch (scause)
    {
        case SCAUSE_INSTRUCTION_ADDR_MISALIGN:
            return "instuction address misaligned";
        case SCAUSE_INSTRUCTION_ACCESS_FAULT: return "instruction access fault";
        case SCAUSE_ILLEGAL_INSTRUCTION: return "illegal instruction";
        case SCAUSE_BREAKPOINT: return "breakpoint";
        case SCAUSE_LOAD_ADDR_MISALIGNED: return "load address misaligned";
        case SCAUSE_LOAD_ACCESS_FAULT: return "load access fault";
        case SCAUSE_STORE_AMO_ADDR_MISALIGN:
            return "store/AMO address misaligned";
        case SCAUSE_STORE_AMO_ACCESS_FAULT: return "store/AMO access fault";
        case SCAUSE_ECALL_FROM_U_MODE: return "environment call from U-mode";
        case SCAUSE_ECALL_FROM_S_MODE: return "environment call from S-mode";
        case 10: return "reserved";
        case 11: return "reserved";
        case SCAUSE_INSTRUCTION_PAGE_FAULT: return "instruction page fault";
        case SCAUSE_LOAD_PAGE_FAULT: return "load page fault";
        case 14: return "reserved";
        case SCAUSE_STORE_AMO_PAGE_FAULT: return "store/AMO page fault";
        case SCAUSE_SOFTWARE_CHECK: return "software check";
        case SCAUSE_HARDWARE_CHECK: return "hardware check";

        case SCAUSE_USER_SOFTWARE_INTERRUPT: return "user software interrupt";
        case SCAUSE_SUPERVISOR_SOFTWARE_INTERRUPT:
            return "supervisor software interrupt";
        case SCAUSE_SUPERVISOR_TIMER_INTERRUPT:
            return "supervisor timer interrupt";
        case SCAUSE_SUPERVISOR_EXTERNAL_INTERRUPT:
            return "supervisor external interrupt";
        case SCAUSE_COUNTER_OVERFLOW_INTERRUPT:
            return "counter overflow interrupt";
        default:
            if (scause & SCAUSE_INTERRUPT_BIT)
            {
                return "unknown interrupt scause";
            }
            else
            {
                return "unknown scause";
            }
    };
}
