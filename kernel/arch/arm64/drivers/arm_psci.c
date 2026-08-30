/* SPDX-License-Identifier: MIT */

#include <arch/arm64/drivers/arm_psci.h>
#include <arch/arm64/drivers/arm_psci_defs.h>
#include <drivers/driver_list.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/reset.h>
#include <kernel/string.h>
#include <libfdt.h>

REGISTER_DRIVER("arm,psci-1.0", arm_psci_init);
REGISTER_DRIVER("arm,psci-0.2", arm_psci_init);
REGISTER_DRIVER("arm,psci", arm_psci_init);

typedef int64_t (*PSCI_CALL_FUNCTION)(uint64_t function_id, uint64_t arg0,
                                      uint64_t arg1, uint64_t arg2);

struct arm_psci_state
{
    bool initialized;
    PSCI_CALL_FUNCTION call_impl;
};

static struct arm_psci_state g_arm_psci = {0};

/// Call the Hypervisor in EL2.
static int64_t arm_psci_call_hvc(uint64_t function_id, uint64_t arg0,
                                 uint64_t arg1, uint64_t arg2)
{
    register uint64_t x0 asm("x0") = function_id;
    register uint64_t x1 asm("x1") = arg0;
    register uint64_t x2 asm("x2") = arg1;
    register uint64_t x3 asm("x3") = arg2;

    asm volatile("hvc #0"
                 : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
                 :
                 : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
                   "x13", "x14", "x15", "x16", "x17", "memory");

    return (int64_t)x0;
}

/// Call the Secure Monitor in EL1.
static int64_t arm_psci_call_smc(uint64_t function_id, uint64_t arg0,
                                 uint64_t arg1, uint64_t arg2)
{
    register uint64_t x0 asm("x0") = function_id;
    register uint64_t x1 asm("x1") = arg0;
    register uint64_t x2 asm("x2") = arg1;
    register uint64_t x3 asm("x3") = arg2;

    asm volatile("smc #0"
                 : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
                 :
                 : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
                   "x13", "x14", "x15", "x16", "x17", "memory");

    return (int64_t)x0;
}

/// Calls the PCSI implementation, either the Hypervisor or the Secure Monitor
/// based on what was defined in the Device Tree.
static int64_t arm_psci_call(uint32_t function_id, uint64_t arg0, uint64_t arg1,
                             uint64_t arg2)
{
    DEBUG_EXTRA_PANIC(g_arm_psci.call_impl != NULL, "no PSCI function set");

    return g_arm_psci.call_impl(function_id, arg0, arg1, arg2);
}

static void arm_psci_machine_power_off()
{
    arm_psci_call(ARM_PSCI_FN_SYSTEM_OFF, 0, 0, 0);
    panic("PSCI system off returned unexpectedly");
}

static void arm_psci_machine_restart()
{
    arm_psci_call(ARM_PSCI_FN_SYSTEM_RESET, 0, 0, 0);
    panic("PSCI system reset returned unexpectedly");
}

PSCI_CALL_FUNCTION arm_psci_parse_method(const void *dtb, int node_offset)
{
    int len = 0;
    const char *method = fdt_getprop(dtb, node_offset, "method", &len);
    if (method == NULL || len < 3)
    {
        return NULL;
    }

    if (strncmp(method, "hvc", 3) == 0)
    {
        return arm_psci_call_hvc;
    }

    if (strncmp(method, "smc", 3) == 0)
    {
        return arm_psci_call_smc;
    }

    return NULL;
}

dev_t arm_psci_init(struct Device_Init_Parameters *init_parameters,
                    const char *name)
{
    (void)name;

    if (g_arm_psci.initialized || (init_parameters == NULL) ||
        (init_parameters->dtb == NULL))
    {
        return INVALID_DEVICE;
    }

    // figure out whether to call PSCI via the Hypervisor or Secure Monitor
    // call:
    g_arm_psci.call_impl = arm_psci_parse_method(init_parameters->dtb,
                                                 init_parameters->dev_offset);

    if (g_arm_psci.call_impl == NULL)
    {
        printk("PSCI: unsupported or missing method property\n");
        return INVALID_DEVICE;
    }

    g_arm_psci.initialized = true;

    int64_t version = arm_psci_call(ARM_PSCI_FN_VERSION, 0, 0, 0);
    printk("PSCI version: %ld.%ld\n", version / 0xFFFF, version % 0xFFFF);

    // register shutdown functions:
    g_machine_power_off_func = &arm_psci_machine_power_off;
    g_machine_restart_func = &arm_psci_machine_restart;

    return MKDEV(MAJOR_ARM_PSCI, 0);
}

bool arm_psci_available() { return g_arm_psci.initialized; }

int64_t arm_psci_cpu_on(uint64_t target_cpu, uint64_t entry_point,
                        uint64_t context_id)
{
    if (!g_arm_psci.initialized)
    {
        return -1;
    }

    return arm_psci_call(ARM_PSCI_FN_CPU_ON, target_cpu, entry_point,
                         context_id);
}
