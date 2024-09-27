/* SPDX-License-Identifier: MIT */

#include <drivers/devices_list.h>
#include <kernel/kernel.h>

/// Shutdown/reboot register
#define VIRT_TEST_SHUTDOWN_REG (0x00)

/// written to VIRT_TEST_SHUTDOWN_REG the machine / qemu should shutdown
#define VIRT_TEST_SHUTDOWN 0x5555

/// written to VIRT_TEST_SHUTDOWN_REG the machine / qemu should reboot
#define VIRT_TEST_REBOOT 0x7777

/// @brief Init function, if never called the shutdown and reboot will panic
/// @param mapping Only the memory address is relevant.
dev_t syscon_init(struct Device_Memory_Map *mapping);

/// @brief Write a 32 bit value to a register.
/// @param reg Register offset in bytes.
/// @param value Unsigned 32 bit value to write.
void syscon_write_reg(size_t reg, uint32_t value);
