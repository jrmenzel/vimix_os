/* SPDX-License-Identifier: MIT */

#include <arch/arm64/drivers/arm_psci.h>
#include <arch/asm.h>
#include <arch/barrier.h>
#include <arch/system.h>
#include <init/dtb.h>
#include <kernel/cpu.h>
#include <kernel/param.h>
#include <kernel/pgtable.h>
#include <kernel/proc.h>
#include <kernel/smp.h>
#include <libfdt.h>
#include <mm/arch_vm.h>
#include <mm/kernel_memory.h>
#include <mm/vm.h>

void cpu_flush_dcache_range(size_t va, size_t len)
{
    if (len == 0)
    {
        return;
    }

    size_t line_size =
        g_cpus[smp_processor_id()].features.dcache_line_size_bytes;
    if (line_size == 0)
    {
        panic("dcache line size unknown\n");
    }
    size_t start = va & ~(line_size - 1);
    size_t end = (va + len + line_size - 1) & ~(line_size - 1);

    for (size_t line = start; line < end; line += line_size)
    {
        asm volatile("dc cvac, %0" : : "r"(line) : "memory");
    }
    smp_mb();
    isb();
}

static bool arm64_spin_table_release(uint64_t release_pa, uint64_t entry_pa)
{
    printk(
        "Releasing secondary CPU with entry point 0x%llx via spin-table "
        "release address 0x%llx\n",
        (unsigned long long)entry_pa, (unsigned long long)release_pa);

    size_t release_va_addr = phys_to_virt((size_t)release_pa);
    volatile uint64_t *release_va64 = (volatile uint64_t *)release_va_addr;

    *release_va64 = entry_pa;

    // Ensure release value visibility before waking secondary cores.
    cpu_flush_dcache_range((size_t)release_va64, sizeof(*release_va64));

    // send event to all cores (here: to wake them up)
    asm volatile("sev" : : : "memory");

    printk("done releasing secondary CPU via spin-table\n");

    return true;
}

void ensure_addr_is_mapped(size_t pa)
{
    spin_lock(&g_kernel_pagetable->lock);
    struct MM_Region *region = memory_map_get_region_at_addr(
        &g_kernel_pagetable->memory_map, phys_to_virt(pa));
    if (region == NULL || region->mapped == MM_REGION_NEVER_MAP)
    {
        size_t ps_page_start = PAGE_ROUND_DOWN(pa);
        memory_map_add_region_and_split(
            &g_kernel_pagetable->memory_map, ps_page_start,
            phys_to_virt(ps_page_start), PAGE_SIZE, MM_REGION_RESERVED_RAM);
        kvm_apply_kernel_mapping(g_kernel_pagetable);

        debug_print_memory_map(&g_kernel_pagetable->memory_map);
    }
    spin_unlock(&g_kernel_pagetable->lock);
}

syserr_t system_boot_cpu(size_t cpu_idx, const void *dtb, size_t start_pa)
{
    const char *cpus_method = dtb_cpus_enable_method(dtb);
    // printk("Booting secondary CPU %zd with method '%s'...\n", cpu_idx,
    //        cpus_method != NULL ? cpus_method : "(none)");

    bool cpus_method_is_bcm2836_smp = false;
    if (cpus_method != NULL)
    {
        cpus_method_is_bcm2836_smp =
            (strncmp(cpus_method, "brcm,bcm2836-smp", 16) == 0);
    }

    int cpu_offset = dtb_get_cpu_offset(dtb, cpu_idx, false);
    if (cpu_offset < 0)
    {
        return -ENODEV;
    }

    uint64_t cpu_target = cpu_idx;
    if (dtb_read_u64_prop(dtb, cpu_offset, "reg", &cpu_target) == false)
    {
        printk(
            "CPU %zd: failed to read 'reg' property, skipping secondary "
            "boot\n",
            cpu_idx);
        return -EOTHER;
    }

    const char *enable_method = dtb_get_nonempty_string_property(
        dtb, cpu_offset, "enable-method", NULL);

    if (cpus_method != NULL && !cpus_method_is_bcm2836_smp)
    {
        enable_method = cpus_method;
    }

    if (enable_method == NULL)
    {
        printk("CPU %zd: no enable-method, skipping secondary boot\n", cpu_idx);
        return -EOTHER;
    }

    bool requested = false;
    size_t psci_ctx_pa = 0;

    // printk("Enable method for CPU %zd: '%s'\n", cpu_idx, enable_method);

    if (strncmp(enable_method, "psci", 4) == 0)
    {
        // printk("CPU %zd: PSCI enable-method requested\n", cpu_idx);

        if (!arm_psci_available())
        {
            printk("CPU %zd: PSCI method requested but PSCI unavailable\n",
                   cpu_idx);
            return -EOTHER;
        }

        psci_ctx_pa = 0;

        int64_t ret = arm_psci_cpu_on(cpu_target, start_pa, psci_ctx_pa);
        if (ret < 0)
        {
            printk("CPU %zd: PSCI CPU_ON failed (%ld)\n", cpu_idx, ret);
            return -EOTHER;
        }

        requested = true;
    }
    else if ((strncmp(enable_method, "spin-table", 10) == 0) ||
             (strncmp(enable_method, "brcm,bcm2836-smp", 16) == 0))
    {
        // printk("CPU %zd: spin-table enable-method requested\n", cpu_idx);

        uint64_t release_pa = 0;
        if (!dtb_read_u64_prop(dtb, cpu_offset, "cpu-release-addr",
                               &release_pa))
        {
            printk("CPU %zd: spin-table without cpu-release-addr\n", cpu_idx);
            return -EOTHER;
        }

        ensure_addr_is_mapped(release_pa);

        requested = arm64_spin_table_release(release_pa, start_pa);
    }
    else
    {
        printk("CPU %zd: unsupported enable-method '%s'\n", cpu_idx,
               enable_method);
        return -EOTHER;
    }

    if (!requested)
    {
        return -EOTHER;
    }

    return 0;
}

bool system_did_cpu_start(size_t cpu_idx)
{
    return (g_cpus[cpu_idx].state == CPU_STARTED);
}
