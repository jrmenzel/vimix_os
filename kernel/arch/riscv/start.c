/* SPDX-License-Identifier: MIT */

#include <arch/riscv/clint.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

void main();

/// entry.S needs one kernel stack per CPU (4KB each)
/// As long as the kernel stack is fixed at 4K, recursion can be deadly.
__attribute__((aligned(16))) char g_kernel_cpu_stack[PAGE_SIZE * MAX_CPUS];

/// entry.S jumps here in machine mode on g_kernel_cpu_stack.
void start()
{
    // set M Previous Privilege mode to Supervisor, for mret.
    xlen_t x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // set M Exception Program Counter to main, for mret.
    // requires gcc -mcmodel=medany
    w_mepc((xlen_t)main);

    // disable paging for now.
    cpu_set_page_table(0);

    // delegate all interrupts and exceptions to supervisor mode.
    w_medeleg(0xffff);
    w_mideleg(0xffff);
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    // ask for clock interrupts.
    clint_init_timer_interrupt();

    // keep each CPU's hartid in its tp register, for smp_processor_id().
    xlen_t id = r_mhartid();
    w_tp(id);

    // switch to supervisor mode and jump to main().
    asm volatile("mret");
}
