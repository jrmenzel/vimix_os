/* SPDX-License-Identifier: MIT */

#include <drivers/syscon.h>
#include <kernel/major.h>

struct Device_Init_Parameters syscon_mapping = {0};
bool syscon_is_initialized = false;

dev_t syscon_init(struct Device_Init_Parameters *init_parameters)
{
    if (syscon_is_initialized)
    {
        return 0;
    }
    syscon_mapping = *init_parameters;
    syscon_is_initialized = true;
    return MKDEV(SYSCON_MAJOR, 0);
}

void syscon_write_reg(size_t reg, uint32_t value)
{
    if (!syscon_is_initialized) return;

    (*(volatile uint32_t *)syscon_mapping.mem_start) = value;
}