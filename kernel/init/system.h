/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

enum Compatible_System
{
    SYSTEM_UNKNOWN = 0,
    SYSTEM_RISCV_QEMU,
    SYSTEM_RISCV_SPIKE,
    SYSTEM_RISCV_VISIONFIVE2,
    SYSTEM_RISCV_ORANGEPI_RV2
};

struct System
{
    const char *model;
    enum Compatible_System compatible;
    const void *dtb;
    const void *boot_dtb;
};

extern struct System g_system;

const void *system_init_from_dtb(const void *dtb);

static inline enum Compatible_System getSystemCompatible()
{
    return g_system.compatible;
}

void system_print_info();

void system_boot_other_cpus(const void *dtb);
