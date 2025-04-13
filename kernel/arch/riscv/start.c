/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <arch/riscv/clint.h>
#include <arch/timer.h>
#include <init/dtb.h>
#include <init/main.h>
#include <init/start.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/page.h>

#ifdef CONFIG_RISCV_BOOT_M_MODE
// Reset CPU to a known state for everything relevant that we can access
// from M-Mode. If the OS starts in S-Mode, the bios managed this low-level
// stuff.
void reset_m_mode_cpu_state()
{
    // delegate all interrupts and exceptions to supervisor mode.
    rv_write_csr_medeleg(0xffff);
    rv_write_csr_mideleg(0xffff);

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
#if (__riscv_xlen == 32)
    rv_write_csr_pmpaddr0(0xffffffff);
#else
    rv_write_csr_pmpaddr0(0x3fffffffffffffull);
#endif
    rv_write_csr_pmpcfg0(PMP_R | PMP_W | PMP_X | PMP_MATCH_NAPOT);

    // allow supervisor to use stimecmp and time.
    rv_write_csr_mcounteren(rv_read_csr_mcounteren() | 2);

#if defined(__RISCV_EXT_SSTC)
    //  enable supervisor-mode timer interrupts.
    rv_write_csr_mie(rv_read_csr_mie() | MIE_STIE);

    // enable the sstc extension (i.e. stimecmp).
#if (__riscv_xlen == 32)
    rv_write_csr_menvcfgh(rv_read_csr_menvcfgh() | (1L << 31));
#else
    rv_write_csr_menvcfg(rv_read_csr_menvcfg() | (1L << 63));
#endif

#endif
}
#else
void reset_m_mode_cpu_state() {}
#endif

#ifdef CONFIG_RISCV_BOOT_M_MODE
// Jump to main() but in Supervisor Mode with the provided parameters
void jump_to_main_in_s_mode(void *dtb, xlen_t is_first)
{
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
    register void *a0 asm("a0") = dtb;
    register xlen_t a1 asm("a1") = is_first;
    asm volatile("mret" : : "r"(a0), "r"(a1));
}
#endif

void cpu_set_boot_state()
{
    reset_m_mode_cpu_state();  ///< noop when booting in S-Mode
    rv_write_csr_sie(0);
}
