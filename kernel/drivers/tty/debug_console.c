/* SPDX-License-Identifier: MIT */

#include <drivers/tty/console.h>
#include <drivers/tty/debug_console.h>
#include <fs/dentry_cache.h>
#include <kernel/proc.h>
#include <kernel/string.h>
#include <mm/kalloc.h>

struct Debug_Console *alloc_init_debug_console(struct Console_Device *parent)
{
    struct Debug_Console *dbg_con =
        kmalloc(sizeof(struct Debug_Console), ALLOC_FLAG_ZERO_MEMORY);

    if (dbg_con == NULL)
    {
        return NULL;
    }
    dbg_con->parent = parent;

    return dbg_con;
}

void dbg_con_activate(struct Debug_Console *dbg_con)
{
    if (dbg_con->is_active) return;

    printk(
        "\n"
        "Enter Debug Console - type 'help' for available commands\n"
        "DBG> ");

    dbg_con->is_active = true;
}

void dbg_con_deactivate(struct Debug_Console *dbg_con)
{
    dbg_con->is_active = false;
    // clear line
    memset(dbg_con->line, 0, DBG_CON_MAX_LINE);
    dbg_con->write_pos = 0;

    printk("\nExit Debug Console\n\n");
}

void dbg_con_print_help()
{
    printk("--- Debug Commands ---\n");
    printk("help:      this help\n");
    printk("plist:     process list\n");
    printk("ptable:    process list with page tables\n");
    printk("pustack:   process list with user stack\n");
    printk("pkstack:   process list with kernel stack\n");
    printk("pfiles:    process list with open files\n");
    printk("inodes:    inodes in file systems\n");
    printk("pagetable: kernel page table (can get long)\n");
    printk("epoch:     page table epochs\n");
    printk("mmap:      memory map\n");
    printk("dcache:    Dentry cache\n");
    printk("kobj:      kobj tree for sysfs\n");
    printk("exit:      exit debug console (or CTRL+H)\n");
}

void debug_print_epochs()
{
    for (size_t i = 0; i < MAX_CPUS; i++)
    {
        if (g_cpus[i].state == CPU_UNUSED) continue;

        printk("CPU %zd: kernel page table epoch seen: %zu\n", i,
               g_cpus[i].kernel_pgtable_epoch_seen);
    }
}

void dbg_con_handle_line(struct Debug_Console *dbg_con)
{
    if (strncmp(dbg_con->line, "help", DBG_CON_MAX_LINE) == 0)
    {
        dbg_con_print_help();
    }
    else if (strncmp(dbg_con->line, "plist", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_process_list(false, false, false, false);
    }
    else if (strncmp(dbg_con->line, "ptable", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_process_list(false, false, false, true);
    }
    else if (strncmp(dbg_con->line, "pustack", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_process_list(true, false, false, false);
    }
    else if (strncmp(dbg_con->line, "pkstack", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_process_list(false, true, false, false);
    }
    else if (strncmp(dbg_con->line, "pfiles", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_process_list(false, false, true, false);
    }
    else if (strncmp(dbg_con->line, "inodes", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_inodes();
    }
    else if (strncmp(dbg_con->line, "pagetable", DBG_CON_MAX_LINE) == 0)
    {
        debug_vm_print_page_table(g_kernel_pagetable);
    }
    else if (strncmp(dbg_con->line, "mmap", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_memory_map(&g_kernel_pagetable->memory_map);
    }
    else if (strncmp(dbg_con->line, "epoch", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_epochs();
    }
    else if (strncmp(dbg_con->line, "dcache", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_dentry_cache();
    }
    else if (strncmp(dbg_con->line, "kobj", DBG_CON_MAX_LINE) == 0)
    {
        debug_print_kobject_tree();
    }
    else if (strncmp(dbg_con->line, "exit", DBG_CON_MAX_LINE) == 0)
    {
        dbg_con_deactivate(dbg_con);
        return;
    }
    else
    {
        printk("Unknown command: %s\n", dbg_con->line);
    }

    // clear line
    memset(dbg_con->line, 0, DBG_CON_MAX_LINE);
    dbg_con->write_pos = 0;
}

void dbg_con_handle_input(struct Debug_Console *dbg_con, uint32_t c)
{
    switch (c)
    {
        case CONTROL_KEY('H'): dbg_con_deactivate(dbg_con); break;
        case DELETE_KEY:
            if (dbg_con->write_pos > 0)
            {
                dbg_con->write_pos--;
                dbg_con->line[dbg_con->write_pos] = 0;
                console_putc(dbg_con->parent, BACKSPACE);
            }
            break;
        case '\n':
        case '\r':
            console_putc(dbg_con->parent, '\n');
            dbg_con_handle_line(dbg_con);
            printk("DBG> ");
            break;
        default:
            if (dbg_con->write_pos <= DBG_CON_MAX_LINE)
            {
                dbg_con->line[dbg_con->write_pos++] = c;
            }
            console_putc(dbg_con->parent, c);
            break;
    }
}
