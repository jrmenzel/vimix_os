/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/param.h>

/**
 * This file contains the defines of major device numbers.
 * Device numbers are encoded in dev_t, see types.h.
 *
 * Linux also has a major.h with device number defines.
 */
#define QEMU_VIRT_IO_DISK_MAJOR 2
#define CONSOLE_DEVICE_MAJOR 1
#define CONSOLE_DEVICE_MINOR 0

#define DEV_NULL_MAJOR 3
#define DEV_NULL_MINOR 0

#define DEV_ZERO_MAJOR 4
#define DEV_ZERO_MINOR 0

#define RAMDISK_MAJOR 5

// NOTE: highest mayor value can be MAX_DEVICES-1

// no uart device for now, the console device embedds the UART
// #define UART_16550_MAJOR         3

// macro values from Linux kdev_t.h:
#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)

/// get the major device ID
#define MAJOR(dev) ((unsigned int)((dev) >> MINORBITS))

/// get the minor device ID
#define MINOR(dev) ((unsigned int)((dev)&MINORMASK))

/// make a dev_t from major and minor device ID
#define MKDEV(ma, mi) (((ma) << MINORBITS) | (mi))
