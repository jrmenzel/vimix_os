/* SPDX-License-Identifier: MIT */
// based on a xv6 driver from Luc Videau (https://github.com/GerfautGE/xv6-mars)

#include <drivers/jh7110_clk.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <libfdt.h>

/// register fields
#define CLK_ENABLE (1 << 31)

struct
{
    bool is_initialized;
    size_t sys_ctl_base;  ///< memory map start sys registers
    size_t stg_ctl_base;  ///< memory map start stg registers
    size_t aon_ctl_base;  ///< memory map start aon registers
} g_jh7110_clk = {0};

dev_t jh7110_clk_init(struct Device_Init_Parameters *init_parameters,
                      const char *name)
{
    if (g_jh7110_clk.is_initialized) return INVALID_DEVICE;

    // find which map is which
    for (size_t i = 0; i < DEVICE_MAX_MEM_MAPS; i++)
    {
        if (init_parameters->mem[i].size == 0) break;

        const char *name = init_parameters->mem[i].name;
        if (name == NULL) continue;

        if (strcmp("sys", name) == 0)
        {
            g_jh7110_clk.sys_ctl_base = init_parameters->mem[i].start;
        }
        else if (strcmp("stg", name) == 0)
        {
            g_jh7110_clk.stg_ctl_base = init_parameters->mem[i].start;
        }
        else if (strcmp("aon", name) == 0)
        {
            g_jh7110_clk.aon_ctl_base = init_parameters->mem[i].start;
        }
    }

    if (g_jh7110_clk.sys_ctl_base == 0)
    {
        // only register range we need so far, so must be set
        return INVALID_DEVICE;
    }

    g_jh7110_clk.is_initialized = true;
    return MKDEV(JH7110_CLK_MAJOR, 0);
}

void jh7110_clk_enable(int num_clk)
{
    if (!g_jh7110_clk.is_initialized)
    {
        panic("clk_enable: jh7110 clk is not initialized");
    }
    size_t reg_offset = num_clk * sizeof(int32_t);
    volatile uint32_t *addr =
        (volatile uint32_t *)(g_jh7110_clk.sys_ctl_base + reg_offset);

    *addr |= CLK_ENABLE;
}

void jh7110_rst_deassert(int num_rst)
{
    if (!g_jh7110_clk.is_initialized)
    {
        panic("rst_deassert: jh7110 clk is not initialized");
    }

    size_t reg_offset = sizeof(int32_t) * (RSTN_BASE + num_rst / 32);
    volatile uint32_t *addr =
        (volatile uint32_t *)(g_jh7110_clk.sys_ctl_base + reg_offset);

    // set only the id%32 bit to 0
    *addr &= ~(1 << (num_rst % 32));
}
