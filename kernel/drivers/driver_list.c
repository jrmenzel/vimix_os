/* SPDX-License-Identifier: MIT */

#include <drivers/driver_list.h>
#include <init/start.h>

struct Driver_List g_driver_list;

void driver_list_init()
{
    g_driver_list.driver = (struct Driver *)__start_driver_list;
    g_driver_list.driver_end = (struct Driver *)__end_driver_list;

    // debug_print_driver_list();
}

void debug_print_driver_list()
{
    printk("---\n");
    for_each_driver(driver)
    {
        printk("driver: %s (%s)\n", driver->dtb_name,
               driver->type == VIRTUAL ? "virtual" : "physical");
    }
    printk("---\n");
}
