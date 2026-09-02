/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>

/// @brief Send a property buffer to Raspberry Pi firmware.
/// @param buffer 16-byte aligned buffer in kernel virtual memory.
/// @param buffer_size Size in bytes, must match the first buffer word.
/// @return true on success, false on timeout/protocol error.
bool bcm2835_firmware_property_call(void *buffer, size_t buffer_size);

/// @brief Read firmware revision via property interface.
/// @param revision_out Optional output pointer.
/// @return true on success.
bool bcm2835_firmware_get_revision(uint32_t *revision_out);

// Clock IDs for Raspberry Pi firmware GET_CLOCK_RATE tag.
#define BCM2835_FIRMWARE_CLOCK_ID_UART 0x00000002u
#define BCM2835_FIRMWARE_CLOCK_ID_CORE 0x00000004u

/// @brief Read a firmware-managed clock rate in Hz.
/// @param clock_id Clock ID (e.g. BCM2835_FIRMWARE_CLOCK_ID_CORE).
/// @param rate_out Optional output pointer.
/// @return true on success.
bool bcm2835_firmware_get_clock_rate(uint32_t clock_id, uint32_t *rate_out);

dev_t bcm2835_firmware_init(struct Device_Init_Parameters *init_parameters,
                            const char *name);
