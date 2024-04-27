/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <arch/riscv/clint.h>
#include <init/main.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <mm/memlayout.h>

void main();

/// entry.S needs one kernel stack per CPU (4KB each)
/// As long as the kernel stack is fixed at 4K, recursion can be deadly.
__attribute__((aligned(16))) char g_kernel_cpu_stack[PAGE_SIZE * MAX_CPUS];

#ifdef __ENABLE_SBI__

/// @brief entry.S jumps here in Supervisor Mode when run on SBI.
///        Only for one hart initially, all other harts are started explicitly
///        via init_platform() - but those also call _entry->start().
/// @param cpuid Hart ID
/// @param device_tree set by SBI to the device tree file, not set for cores
/// started by sbi_hart_start()
void start(xlen_t cpuid, void *device_tree)
{
    // no race condition thanks to how harts are started in SBI mode
    bool is_first_thread = g_global_init_done == GLOBAL_INIT_NOT_STARTED;

    // disable paging for now.
    cpu_set_page_table(0);
    rv_write_csr_sie(rv_read_csr_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // keep each CPU's hartid in its tp register, for smp_processor_id().
    w_tp(cpuid);

    // SBI timer interrupt
    sbi_set_timer();

    main(device_tree, is_first_thread);
}

#else

/// entry.S jumps here in Machine Mode on g_kernel_cpu_stack[PAGE_SIZE * cpu_id]
/// First C function to run on each hart in Machine Mode.
/// Parameters may or may not be provided by the boot loader.
void start(xlen_t cpuid, void *device_tree)
{
    bool is_first_thread = rv_read_csr_mhartid() == 0;

    // disable paging for now.
    cpu_set_page_table(0);

    // delegate all interrupts and exceptions to supervisor mode.
    rv_write_csr_medeleg(0xffff);
    rv_write_csr_mideleg(0xffff);
    rv_write_csr_sie(rv_read_csr_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
#if (__riscv_xlen == 32)
    rv_write_csr_pmpaddr0(0xffffffff);
#else
    rv_write_csr_pmpaddr0(0x3fffffffffffffull);
#endif
    rv_write_csr_pmpcfg0(PMP_R | PMP_W | PMP_X | PMP_MATCH_NAPOT);

    // keep each CPU's hartid in its tp register, for smp_processor_id().
    w_tp(rv_read_csr_mhartid());

    // ask for clock interrupts.
    clint_init_timer_interrupt();

    // Jump to main() but in Supervisor Mode
    //
    // 1. set M Exception Program Counter to main, so mret will jump there
    //    (requires gcc -mcmodel=medany)
    rv_write_csr_mepc((xlen_t)main);

    // 2. set M Previous Privilege mode to Supervisor, so mret will switch to S
    // mode
    xlen_t machine_status = rv_read_csr_mstatus();
    machine_status &= ~MSTATUS_MPP_MASK;
    machine_status |= MSTATUS_MPP_S;
    rv_write_csr_mstatus(machine_status);

    // 3. call mret to switch to supervisor mode and jump to main()
    register void *a0 asm("a0") = device_tree;
    register xlen_t a1 asm("a1") = is_first_thread;
    asm volatile("mret" : : "r"(a0), "r"(a1));
}

#endif  // __ENABLE_SBI__
