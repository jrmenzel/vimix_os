/* SPDX-License-Identifier: MIT */

#pragma once

#include <drivers/devices_list.h>

#define BCM2835_MBOX_CHANNEL_PROPERTY_TAGS 8

dev_t bcm2835_mbox_init(struct Device_Init_Parameters *init_parameters,
                        const char *name);

/// @brief Send one mailbox message and wait for a response on the same channel.
/// @param channel Mailbox channel (0..15).
/// @param data Upper 28 bits of message (lower 4 bits must be 0/aligned).
/// @param response Optional response word.
/// @return true on success, false on timeout/error.
bool bcm2835_mbox_call(uint8_t channel, uint32_t data, uint32_t *response);
