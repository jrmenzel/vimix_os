/* SPDX-License-Identifier: MIT */
#ifdef __ENABLE_SBI__

#include <clint.h>
#include <drivers/console.h>
#include <kernel/kernel.h>
#include <kernel/smp.h>
#include <riscv.h>
#include <sbi.h>

void _entry();

struct sbiret sbi_ecall(int32_t ext, int32_t fid, xlen_t arg0, xlen_t arg1,
                        xlen_t arg2, xlen_t arg3, xlen_t arg4, xlen_t arg5)
{
    struct sbiret ret;

    register xlen_t a0 asm("a0") = arg0;
    register xlen_t a1 asm("a1") = arg1;
    register xlen_t a2 asm("a2") = arg2;
    register xlen_t a3 asm("a3") = arg3;
    register xlen_t a4 asm("a4") = arg4;
    register xlen_t a5 asm("a5") = arg5;
    register xlen_t a6 asm("a6") = fid;
    register xlen_t a7 asm("a7") = ext;
    asm volatile("ecall"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                 : "memory");

    ret.error = a0;
    ret.value = a1;

    return ret;
}

void sbi_console_putchar(int ch)
{
    sbi_ecall(SBI_LEGACY_EXT_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

long sbi_console_getchar()
{
    struct sbiret ret =
        sbi_ecall(SBI_LEGACY_EXT_CONSOLE_GETCHAR, 0, 0, 0, 0, 0, 0, 0);
    return ret.error;  // legacy SBI calls return in a0 -> ret.error
}

void sbi_console_poll_input()
{
    // read and process incoming characters.
    while (true)
    {
        long c = sbi_console_getchar();
        if (c == -1)
        {
            break;
        }
        console_interrupt_handler(c);
    }
}

static inline long sbi_get_spec_version()
{
    struct sbiret ret;

    ret =
        sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0);
    // no error check as base extension functions can not fail, from the spec:
    // "All functions in the base extension must be supported by all SBI
    // implementations, so there are no error returns defined."
    return ret.value;
}

/// @brief Tests if a SBI extension is available.
/// See https://www.scs.stanford.edu/~zyedidia/docs/riscv/riscv-sbi.pdf
/// @param extid Extension ID from the SBI spec
/// @return 1 if the extension is available, 0 otherwise
long sbi_probe_extension(int32_t extid)
{
    struct sbiret ret;

    ret = sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_PROBE_EXTENSION, extid, 0, 0, 0,
                    0, 0);
    // no error check as base extension functions can not fail
    return ret.value;
}

/// @brief Starts a hart
/// @param hartid The hart ID to start
/// @param saddr The function to run
/// @param opaque Opaque parameter to be set in a1
/// @return SBI_SUCCESS on success
static inline long sbi_hart_start(size_t hartid, size_t saddr, size_t opaque)
{
    struct sbiret ret;

    ret = sbi_ecall(SBI_EXT_ID_HSM, SBI_HSM_HART_START, hartid, saddr, opaque,
                    0, 0, 0);
    if (ret.error)
    {
        return ret.error;
    }
    return ret.value;
}

void sbi_set_timer(uint64_t stime_value)
{
#if (__riscv_xlen == 32)
    sbi_ecall(SBI_EXT_ID_TIME, SBI_TIME_SET_TIMER, stime_value,
              stime_value >> 32, 0, 0, 0, 0);
#else
    sbi_ecall(SBI_EXT_ID_TIME, SBI_TIME_SET_TIMER, stime_value, 0, 0, 0, 0, 0);
#endif
}

void init_sbi()
{
    long version = sbi_get_spec_version();
    long major =
        (version >> SBI_SPEC_VERSION_MAJOR_SHIFT) & SBI_SPEC_VERSION_MAJOR_MASK;
    long minor = version & SBI_SPEC_VERSION_MINOR_MASK;
    printk("SBI specification v%ld.%ld detected\n", major, minor);

#if defined(__SBI_CONSOLE__)
    if (sbi_probe_extension(SBI_LEGACY_EXT_CONSOLE_PUTCHAR) <= 0)
    {
        panic(
            "Error: support for the SBI legacy console extension was "
            "expected\n");
    }
#endif
    /*
            if (sbi_probe_extension(SBI_EXT_ID_TIME) > 0)
            {
                // timer extension
                printk("SBI TIME extension detected\n");
            }

            if (sbi_probe_extension(SBI_EXT_ID_IPI) > 0)
            {
                // inter processor interrupts
                printk("SBI IPI extension detected\n");
            }

            if (sbi_probe_extension(SBI_EXT_ID_RFNC) > 0)
            {
                // remote fences
                printk("SBI RFNC extension detected\n");
            }
        */

    if (sbi_probe_extension(SBI_EXT_ID_HSM) > 0)
    {
        //   hart state management
        printk("SBI HSM extension detected, starting additional harts\n");

        size_t this_hart = smp_processor_id();
        size_t hartid = 0;
        while (hartid < MAX_CPUS)
        {
            if (hartid != this_hart)
            {
                // start all other harts, trying to start a hart that does not
                // exist in hardware will fail
                long ret = sbi_hart_start(hartid, (size_t)_entry, 0);
                if (ret != SBI_SUCCESS)
                {
                    // printk("SBI HSM: starting hart %zd: FAILED\n", hartid);
                }
                else
                {
                    printk("SBI HSM: starting hart %zd: OK\n", hartid);
                }
            }
            else
            {
                printk("SBI HSM: hart %zd already running\n", hartid);
            }
            hartid++;
        }
    }
    /*
        if (sbi_probe_extension(SBI_EXT_ID_SRST) > 0)
        {
            printk("SBI SRST extension detected\n");
        }

        if (sbi_probe_extension(SBI_EXT_ID_PMU) > 0)
        {
            //
       github.com/riscv-software-src/opensbi/blob/master/docs/pmu_support.md
            // "SBI PMU extension supports allow supervisor software to
            // configure/start/stop any performance counter at anytime."
            printk("SBI PMU extension detected\n");
        }
    */
    printk("\n");
}

#endif  // __ENABLE_SBI__
