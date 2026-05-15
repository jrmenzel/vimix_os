/* SPDX-License-Identifier: MIT */

#include <arch/system.h>
#include <init/dtb.h>
#include <init/embedded_dtbs.h>
#include <init/system.h>
#include <kernel/kticks.h>
#include <kernel/smp.h>
#include <kernel/time.h>

struct System_Description
{
    const char *compatible_str;
};

struct System_Description g_compatible_systems[] = {
    [SYSTEM_RISCV_QEMU] = {"riscv-virtio"},
    [SYSTEM_RISCV_SPIKE] = {"ucbbar,spike-bare-dev"},
    [SYSTEM_RISCV_VISIONFIVE2] = {"starfive,jh7110"},
    [SYSTEM_RISCV_ORANGEPI_RV2] = {"ky,x1"},
    [SYSTEM_UNKNOWN] = {"unknown"}};

struct System g_system = {.model = "unknown",
                          .compatible = SYSTEM_UNKNOWN,
                          .dtb = NULL,
                          .boot_dtb = NULL};

enum Compatible_System dtb_get_compatible_from_dtb(const void *dtb)
{
    const char *compatible = fdt_getprop(dtb, 0, "compatible", NULL);
    if (compatible != NULL)
    {
        for (size_t i = 0; i < sizeof(g_compatible_systems) /
                                   sizeof(struct System_Description);
             i++)
        {
            struct System_Description *machine_desc = &g_compatible_systems[i];
            if (dtb_is_str_in_str_list(compatible,
                                       machine_desc->compatible_str))
            {
                return i;
            }
        }
    }
    return SYSTEM_UNKNOWN;
}

const void *system_init_from_dtb(const void *dtb)
{
    g_system.dtb = dtb;
    g_system.boot_dtb = dtb;

    const char *model = fdt_getprop(dtb, 0, "model", NULL);
    if (model != NULL)
    {
        g_system.model = model;
    }

    g_system.compatible = dtb_get_compatible_from_dtb(dtb);
    const void *embedded_dtb = get_embedded_dtb(g_system.compatible);
    if (embedded_dtb != NULL)
    {
        g_system.dtb = embedded_dtb;
    }

    arch_init_system();

    return g_system.dtb;
}

void system_print_info()
{
    printk("System model: %s\n", g_system.model);
    printk("System compatible: %s\n",
           g_compatible_systems[g_system.compatible].compatible_str);
    if (g_system.dtb != g_system.boot_dtb)
    {
        printk("Using embedded DTB\n");
    }
    else
    {
        printk("Using boot loader provided DTB\n");
    }
}

void system_boot_other_cpus(const void *dtb)
{
    size_t this_hart = smp_processor_id();
    size_t hartid = 0;
    while (hartid < MAX_CPUS)
    {
        if (hartid != this_hart)
        {
            syserr_t err = system_boot_cpu(hartid, dtb);
            if (err != 0)
            {
                break;
            }

            size_t t0 = msec_since_boot();
            size_t t1 = t0;
            while (t1 - t0 < 1000)
            {
                if (system_did_cpu_start(hartid)) break;
                t1 = msec_since_boot();
            }
            if (system_did_cpu_start(hartid) == false)
            {
                printk("CPU %zd failed to start within timeout\n", hartid);
            }
        }
        hartid++;
    }
}
