/* SPDX-License-Identifier: MIT */

#include <drivers/devices_list.h>
#include <kernel/kernel.h>

/// Shutdown/reboot register
#define VIRT_TEST_SHUTDOWN_REG (0x00)

/// written to VIRT_TEST_SHUTDOWN_REG the machine / qemu should shutdown
#define VIRT_TEST_SHUTDOWN 0x5555

/// written to VIRT_TEST_SHUTDOWN_REG the machine / qemu should reboot
#define VIRT_TEST_REBOOT 0x7777

/// @brief Init function, if never called the shutdown and reboot will panic if
/// this is the only way to shutdown.
/// @param init_parameters Only the memory address is relevant.
/// @param name Device name from the dtb file (if one driver supports multiple
/// devices)
/// @return Device number of successful init.
dev_t syscon_init(struct Device_Init_Parameters *init_parameters,
                  const char *name);

/// @brief Write a 32 bit value to a register.
/// @param reg Register offset in bytes.
/// @param value Unsigned 32 bit value to write.
void syscon_write_reg(size_t reg, uint32_t value);
