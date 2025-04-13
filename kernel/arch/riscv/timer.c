/* SPDX-License-Identifier: MIT */

#include <arch/riscv/clint.h>
#include <arch/riscv/sbi.h>
#include <arch/timer.h>
#include <init/dtb.h>

timer_schedule_interrupt_p *timer_schedule_interrupt = NULL;

/// read from the device tree
uint64_t g_timebase_frequency = 0;

extern uint64_t g_boot_time;

//
// schedule interrupt functions of supported timers

void no_op() {}

#if defined(CONFIG_RISCV_SBI)
void sbi_schedule_interrupt(uint64_t next_timer_intr)
{
    sbi_set_timer(next_timer_intr);
}
#endif

#if defined(__RISCV_EXT_SSTC)
void sstc_schedule_interrupt(uint64_t next_timer_intr)
{
    rv_set_stimecmp(next_timer_intr);
}
#else
void sstc_schedule_interrupt(uint64_t) { panic("sstc unsupported"); }
#endif

//
// init one of the supported timers

void timer_init(uint64_t timebase_frequency, void *dtb, size_t cpuid)
{
    g_timebase_frequency = timebase_frequency;
    g_boot_time = rv_get_time();

#if defined(CONFIG_RISCV_BOOT_M_MODE)
    // bare metal environment
    // ask for clock interrupts.
    clint_init_timer_interrupt();
    timer_schedule_interrupt = no_op;
#elif defined(CONFIG_RISCV_SBI)
    CPU_Features features = dtb_get_cpu_features(dtb, cpuid);
    // SBI environment
    if (features & RV_EXT_SSTC)
    {
        // preferred: sstc extension
        timer_schedule_interrupt = sstc_schedule_interrupt;
    }
    else if (sbi_probe_extension(SBI_EXT_ID_TIME) > 0)
    {
        // fallback: SBI timer
        timer_schedule_interrupt = sbi_schedule_interrupt;
    }

    uint64_t timer_interrupt_interval =
        g_timebase_frequency / TIMER_INTERRUPTS_PER_SECOND;
    uint64_t now = rv_get_time();
    timer_schedule_interrupt(now + timer_interrupt_interval);
#else
#error "unsupported config"
#endif
}
