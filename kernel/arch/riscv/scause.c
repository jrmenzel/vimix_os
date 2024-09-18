/* SPDX-License-Identifier: MIT */

#include "scause.h"

#include <kernel/proc.h>
#include <kernel/vm.h>
#include <mm/memlayout.h>

void dump_scause()
{
    xlen_t scause = rv_read_csr_scause();
    xlen_t stval = rv_read_csr_stval();

    printk("scause (0x%p): %s\n", scause,
           scause_exception_code_to_string(scause));
    printk("sepc=0x%p stval=0x%p\n", rv_read_csr_sepc(), stval);

    if (scause == SCAUSE_INSTRUCTION_PAGE_FAULT ||
        scause == SCAUSE_LOAD_PAGE_FAULT ||
        scause == SCAUSE_STORE_AMO_PAGE_FAULT)
    {
        printk("Tried to ");
        if (scause == SCAUSE_INSTRUCTION_PAGE_FAULT)
        {
            printk("execute from");
        }
        else if (scause == SCAUSE_LOAD_PAGE_FAULT)
        {
            printk("read from");
        }
        else if (scause == SCAUSE_STORE_AMO_PAGE_FAULT)
        {
            printk("write to");
        }
        // stval is set to the offending memory address
        printk(" address 0x%x%s\n", stval,
               (stval ? "" : " (dereferenced NULL pointer)"));

        struct process *proc = get_current();
        if (proc)
        {
#if defined(_arch_is_64bit)
            if (stval >= MAXVA)
            {
                printk("Address 0x%x larger than supported\n", stval);
                return;
            }
#endif

            pte_t *pte = vm_walk(proc->pagetable, stval, false);
            if (!pte)
            {
                printk("Page of address 0x%x is not mapped\n", stval);
            }
            else
            {
                printk("Page of address 0x%x access: ", stval);
                debug_vm_print_pte_flags(*pte);
                printk("\n");
            }
        }
    }
}

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
