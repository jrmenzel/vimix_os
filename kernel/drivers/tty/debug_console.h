/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

#define DBG_CON_MAX_LINE 60

struct Console_Device;

struct Debug_Console
{
    bool is_active;
    size_t write_pos;
    char line[DBG_CON_MAX_LINE];
    struct Console_Device *parent;
};

struct Debug_Console *alloc_init_debug_console(struct Console_Device *parent);

void dbg_con_activate(struct Debug_Console *dbg_con);

void dbg_con_handle_input(struct Debug_Console *dbg_con, uint32_t c);
