/* SPDX-License-Identifier: MIT */

#include <drivers/jh7110_syscrg.h>
#include <drivers/mmio_access.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <libfdt.h>

/// register fields
#define CLK_ENABLE (1 << 31)

struct
{
    bool is_initialized;
    size_t mmio_base;  ///< memory map start
} g_jh7110_syscrg = {0};

dev_t jh7110_syscrg_init(struct Device_Init_Parameters *init_parameters,
                         const char *name)
{
    if (g_jh7110_syscrg.is_initialized) return INVALID_DEVICE;

    printk("syscrg init\n");
    g_jh7110_syscrg.mmio_base = init_parameters->mem[0].start;

    g_jh7110_syscrg.is_initialized = true;
    return MKDEV(JH7110_SYSCRG_MAJOR, 0);
}

void jh7110_syscrg_enable(int num_clk)
{
    if (!g_jh7110_syscrg.is_initialized)
    {
        panic("clk_enable: jh7110 clk is not initialized");
    }

    size_t reg_offset = num_clk * sizeof(int32_t);

    uint32_t setting = MMIO_READ_UINT_32(g_jh7110_syscrg.mmio_base, reg_offset);
    setting |= CLK_ENABLE;
    MMIO_WRITE_UINT_32(g_jh7110_syscrg.mmio_base, reg_offset, setting);
}

void jh7110_syscrg_deassert(int num_rst)
{
    if (!g_jh7110_syscrg.is_initialized)
    {
        panic("rst_deassert: jh7110 clk is not initialized");
    }

    size_t reg_offset = sizeof(int32_t) * (RSTN_BASE + num_rst / 32);
    uint32_t setting = MMIO_READ_UINT_32(g_jh7110_syscrg.mmio_base, reg_offset);
    // set only the id%32 bit to 0
    setting &= ~(1 << (num_rst % 32));
    MMIO_WRITE_UINT_32(g_jh7110_syscrg.mmio_base, reg_offset, setting);
}
