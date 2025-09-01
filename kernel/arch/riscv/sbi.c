/* SPDX-License-Identifier: MIT */

#include <drivers/console.h>
#include <init/dtb.h>
#include <kernel/ipi.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/reset.h>
#include <kernel/smp.h>
#include <plic.h>
#include <riscv.h>
#include <sbi.h>

void _entry_s_mode();

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

void sbi_console_putchar(int32_t ch)
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

static inline struct sbiret sbi_get_impl_id()
{
    return sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_GET_IMPL_ID, 0, 0, 0, 0, 0, 0);
}

static inline struct sbiret sbi_get_impl_version()
{
    return sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_GET_IMPL_VERSION, 0, 0, 0, 0, 0,
                     0);
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

static inline struct sbiret sbi_get_mvendorid()
{
    return sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_GET_MVENDORID, 0, 0, 0, 0, 0, 0);
}

static inline struct sbiret sbi_get_marchid()
{
    return sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_GET_MARCHID, 0, 0, 0, 0, 0, 0);
}

static inline struct sbiret sbi_get_mimpid()
{
    return sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_GET_MIMPID, 0, 0, 0, 0, 0, 0);
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

static inline struct sbiret sbi_hart_get_status(size_t hartid)
{
    return sbi_ecall(SBI_EXT_ID_HSM, SBI_HSM_HART_STATUS, hartid, 0, 0, 0, 0,
                     0);
}

void sbi_print_hart_stati()
{
    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        struct sbiret ret = sbi_hart_get_status(i);
        printk("hart %zd status: %ld %ld\n", i, ret.error, ret.value);
    }
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

bool EXT_SRST_SUPPORTED = false;
bool EXT_SRST_QUERIED = false;

void sbi_system_reset(uint32_t reset_type, uint32_t reset_reason)
{
    if (!EXT_SRST_QUERIED)
    {
        EXT_SRST_SUPPORTED = (sbi_probe_extension(SBI_EXT_ID_SRST) > 0);
    }
    if (!EXT_SRST_SUPPORTED) return;

    sbi_ecall(SBI_EXT_ID_SRST, SBI_SRST_SYSTEM_RESET, reset_type, reset_reason,
              0, 0, 0, 0);
}

void sbi_machine_power_off()
{
    sbi_system_reset(SBI_SRST_TYPE_SHUTDOWN, SBI_SRST_REASON_NONE);
}

void sbi_machine_restart()
{
    sbi_system_reset(SBI_SRST_TYPE_WARM_REBOOT, SBI_SRST_REASON_NONE);
}

struct sbiret sbi_send_ipi(unsigned long hart_mask,
                           unsigned long hart_mask_base)
{
    return sbi_ecall(SBI_EXT_ID_IPI, SBI_IPI_SEND_IPI, hart_mask,
                     hart_mask_base, 0, 0, 0, 0);
}

void init_sbi()
{
    struct sbiret ret;
    ret = sbi_get_impl_id();
    printk("SBI implementation: ");
    switch (ret.value)
    {
        case SBI_IMPL_ID_BBL: printk("Berkley Boot Loader"); break;
        case SBI_IMPL_ID_OPENSBI: printk("OpenSBI"); break;
        case SBI_IMPL_ID_XVISOR: printk("Xvisor"); break;
        case SBI_IMPL_ID_KVM: printk("KVM"); break;
        case SBI_IMPL_ID_RUSTSBI: printk("RustSBI"); break;
        case SBI_IMPL_ID_DIOSIX: printk("Diosix"); break;
        case SBI_IMPL_ID_COFFER: printk("Coffer"); break;
        case SBI_IMPL_ID_XEN: printk("Xen Project"); break;
        case SBI_IMPL_ID_POLARFIRE_HSS:
            printk("Polar Fire Hart Software Services");
            break;
        case SBI_IMPL_ID_COREBOOT: printk("coreboot"); break;
        case SBI_IMPL_ID_OREBOOT: printk("oreboot"); break;
        case SBI_IMPL_ID_BHYVE: printk("bhyve"); break;
        case SBI_IMPL_ID_VIMIX: printk("VIMIX build-in"); break;
        default: printk("0x%lx", ret.value);
    }
    ret = sbi_get_impl_version();
    printk(" (version %ld)\n", ret.value);

    long version = sbi_get_spec_version();
    long major =
        (version >> SBI_SPEC_VERSION_MAJOR_SHIFT) & SBI_SPEC_VERSION_MAJOR_MASK;
    long minor = version & SBI_SPEC_VERSION_MINOR_MASK;
    printk("SBI specification: v%ld.%ld\n", major, minor);

    if (!EXT_SRST_QUERIED)
    {
        EXT_SRST_SUPPORTED = (sbi_probe_extension(SBI_EXT_ID_SRST) > 0);
    }
    if (EXT_SRST_SUPPORTED)
    {
        printk(
            "SBI extension SRST detected: register SBI reboot/shutdown "
            "functions\n");
        g_machine_power_off_func = &sbi_machine_power_off;
        g_machine_restart_func = &sbi_machine_restart;
    }
}

void sbi_start_harts(size_t opaque)
{
    if (sbi_probe_extension(SBI_EXT_ID_HSM) <= 0)
    {
        printk("SBI HSM extension not present, staying single core\n");
        return;
    }

    printk("starting additional harts via SBI HSM extension\n");

    size_t this_hart = smp_processor_id();
    size_t hartid = 0;
    while (hartid < MAX_CPUS)
    {
        if (hartid != this_hart)
        {
            if (plic_get_hart_s_context(hartid) != -1)
            {
                //  CPU exists in device tree and supports s mode interrupts
                long ret =
                    sbi_hart_start(hartid, (size_t)_entry_s_mode, opaque);
                if (ret != SBI_SUCCESS)
                {
                    printk("SBI HSM: starting hart %zd: FAILED\n", hartid);
                }

                // busy wait for the core to have started before requesting the
                // next one, but don't wait forever. Without this not all cores
                // on Orange Pi RV2 start completely (they get stuck in
                // SBI_HSM_HART_START_PENDING).
                for (size_t loop = 0; loop < 1024; ++loop)
                {
                    struct sbiret sret = sbi_hart_get_status(hartid);
                    if (sret.value == SBI_HSM_HART_STARTED) break;
                }
            }
        }
        hartid++;
    }
}

void ipi_send_interrupt(cpu_mask mask, enum ipi_type type, void *data)
{
    spin_lock(&g_cpus_ipi_lock);
    for (size_t i = 0; i < MAX_CPUS; ++i)
    {
        if (mask & (1 << i))
        {
            struct cpu *c = &g_cpus[i];

            // error check
            if (c->state == CPU_UNUSED)
            {
                printk("IPI: CPU %zd not started, cannot send IPI %d\n", i,
                       type);
                continue;
            }

            // find pending IPI count
            size_t pending_count = 0;
            while ((pending_count < MAX_IPI_PENDING) &&
                   (c->ipi[pending_count].pending != IPI_NONE))
            {
                pending_count++;
            }

            // if exactly the same IPI is already pending, don't add
            // another one
            if ((pending_count != 0) &&
                (c->ipi[pending_count - 1].pending == type) &&
                (c->ipi[pending_count - 1].data == data))
            {
                continue;
            }

            if (pending_count == MAX_IPI_PENDING)
            {
                printk("IPI queue full on CPU %zd, dropping IPI %d\n", i, type);
            }
            else
            {
                c->ipi[pending_count].pending = type;
                c->ipi[pending_count].data = data;
            }
        }
    }
    spin_unlock(&g_cpus_ipi_lock);

    struct sbiret ret;

#if (__riscv_xlen == 32)
    unsigned long mask0 = (unsigned long)(mask & 0xFFFFFFFF);
    unsigned long mask1 = (unsigned long)(mask >> 32);
    ret = sbi_send_ipi(mask0, 0);
    if ((ret.error == SBI_SUCCESS) && (mask1 != 0))
        ret = sbi_send_ipi(mask1, 31);
#else
    ret = sbi_send_ipi((unsigned long)mask, 0);
#endif

    if (ret.error)
    {
        printk("SBI IPI send failed: %ld\n", ret.error);
    }
}
