/* SPDX-License-Identifier: MIT */

#include <arch/interrupts.h>
#include <arch/riscv/clint.h>
#include <arch/riscv/timer.h>
#include <init/dtb.h>
#include <init/main.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/kticks.h>
#include <kernel/string.h>

/// entry.S needs one kernel stack per CPU (one page of 4KB each)
/// As long as the kernel stack is fixed at 4K, recursion can be deadly.
/// Placed in section STACK to not be placed in .bss (see kernel.ld).
/// This is because the .bss will be cleared by C code already relying on the
/// stack.
__attribute__((aligned(PAGE_SIZE))) __attribute__((
    section("STACK"))) char g_kernel_cpu_stack[KERNEL_STACK_SIZE * MAX_CPUS];

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

/// @brief Clears the BSS section (uninitialized and Zero-initialized C
/// variables) with zeros.
///        All kernel threads should call this as early as possible (before
///        reading or writing any variables that could be in BSS), but only one
///        should perform the clear. All other threads will wait.
///        Note that the kernel stack is not in bss (see kernel.ld) - if it
///        would be, the bss clear would have to be done from assembly before
///        jumping to C.
/// @param this_thread_clears Should be true for only one thread.
void wait_on_bss_clear(bool this_thread_clears)
{
    if (this_thread_clears)
    {
        // clears the .bss section with zeros
        size_t size = (size_t)bss_end - (size_t)bss_start;
        memset((void *)bss_start, 0, size);

        g_global_init_done = GLOBAL_INIT_BSS_CLEAR;
    }
    else
    {
        while (g_global_init_done < GLOBAL_INIT_BSS_CLEAR)
        {
            __sync_synchronize();
        }
    }
}

void reset_s_mode_cpu_state()
{
    // disable paging for now.
    cpu_set_page_table(0);

    // disable interrupts
    rv_write_csr_sie(0);
    cpu_disable_device_interrupts();
}

/// @brief entry.S jumps here in Supervisor Mode when run on SBI or Machine
///        Mode otherwise. Stack is on g_kernel_cpu_stack[KERNEL_STACK_SIZE *
///        cpu_id]. In M-Mode all cores start at the same time, pick ID 0 as the
///        main thread. On SBI only one hart starts initially, all other harts
///        are started explicitly via init_platform() - but those also call
///        _entry->start().
/// @param cpuid Hart ID
/// @param device_tree set by SBI to the device tree file, not set for cores
/// started by sbi_hart_start()
void start(xlen_t cpuid, void *device_tree)
{
#ifdef __PLATFORM_VISIONFIVE2
    // device tree is not set as expected, but the location is static:
    device_tree = (void *)0xfffc6080;
#endif

    // Pick thread for all non-parallel init stuff:
    // - Based on global flag for SBI (no race condition thanks to how harts are
    // started in SBI mode)
    // - Based on CPU ID otherwise
#ifdef CONFIG_RISCV_SBI
    bool is_first_thread = g_global_init_done == GLOBAL_INIT_NOT_STARTED;
#else
    bool is_first_thread = (cpuid == 0);
#endif  // CONFIG_RISCV_SBI

    reset_m_mode_cpu_state();  ///< noop when booting in S-Mode
    reset_s_mode_cpu_state();

    // clear BSS
    wait_on_bss_clear(is_first_thread);

    if (is_first_thread)
    {
        kticks_init();
    }

#if defined(__RISCV_EXT_SSTC)
    CPU_Features features = dtb_get_cpu_features(device_tree, cpuid);
    bool use_sstc_timer = features & RV_EXT_SSTC;
#else
    // ignore what the hardware can if no support was compiled in
    bool use_sstc_timer = false;
#endif

    // If using CLINT, timer init must be done in M-Mode before jumping to
    // main, also init S-Mode timers here.
    uint64_t timebase = dtb_get_timebase(device_tree);
    timer_init(timebase, use_sstc_timer);

    // enable external, timer, software interrupts
    rv_write_csr_sie(rv_read_csr_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

#if defined(CONFIG_RISCV_BOOT_M_MODE)
    jump_to_main_in_s_mode(device_tree, is_first_thread);
#else
    main(device_tree, is_first_thread);
#endif
}
