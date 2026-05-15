/* SPDX-License-Identifier: MIT */

#include <arch/riscv/sbi.h>
#include <arch/riscv/sbi_defs.h>
#include <kernel/smp.h>
#include <kernel/timer.h>

//
// schedule interrupt functions of supported timers

void sbi_schedule_interrupt(uint64_t time, uint64_t interval)
{
    sbi_set_timer(time + interval);
}

#if defined(__RISCV_EXT_SSTC)
void sstc_schedule_interrupt(uint64_t time, uint64_t interval)
{
    rv_set_stimecmp(time + interval);
}
#else
void sstc_schedule_interrupt(uint64_t, uint64_t) { panic("sstc unsupported"); }
#endif

//
// init one of the supported timers

timer_schedule_interrupt_p *arch_timer_interrupt_func(CPU_Features features)
{
    if (features & RV_EXT_SSTC)
    {
        // preferred: sstc extension
        return sstc_schedule_interrupt;
    }
    else if (sbi_probe_extension(SBI_EXT_ID_TIME) > 0)
    {
        // fallback: SBI timer
        return sbi_schedule_interrupt;
    }

    return NULL;
}
