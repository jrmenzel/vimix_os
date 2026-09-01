/* SPDX-License-Identifier: MIT */

#include <drivers/bcm/bcm2835_firmware.h>
#include <drivers/bcm/bcm2835_mbox.h>
#include <kernel/kticks.h>
#include <kernel/major.h>
#include <kernel/pgtable.h>
#include <kernel/reset.h>
#include <kernel/spinlock.h>

REGISTER_DRIVER("raspberrypi,bcm2835-firmware", bcm2835_firmware_init);

#define RPI_FIRMWARE_STATUS_SUCCESS 0x80000000u
#define RPI_FIRMWARE_TAG_GET_FIRMWARE_REVISION 0x00000001u
#define RPI_FIRMWARE_TAG_GET_CLOCK_RATE 0x00030002u
#define RPI_FIRMWARE_TAG_SET_POWER_STATE 0x00028001u

#define RPI_FIRMWARE_DEVICE_ID_SD_CARD 0x00000000u
#define RPI_FIRMWARE_SET_POWER_STATE_WAIT 0x00000002u

struct bcm2835_firmware
{
    struct spinlock lock;
    bool isInitialized;
};

struct bcm2835_firmware g_bcm2835_firmware = {0};

void bcm2835_firmware_power_off();

dev_t bcm2835_firmware_init(struct Device_Init_Parameters *init_param,
                            const char *name)
{
    (void)init_param;
    (void)name;

    struct Devices_List *dev_list = get_devices_list();
    bool mbox_init = init_device_by_name(dev_list, "brcm,bcm2835-mbox");
    if (!mbox_init) return INVALID_DEVICE;

    spin_lock_init(&g_bcm2835_firmware.lock, "bcm2835_firmware_lock");
    g_bcm2835_firmware.isInitialized = true;

    uint32_t fw_rev = 0;
    if (bcm2835_firmware_get_revision(&fw_rev))
    {
        printk("bcm2835 firmware revision: 0x%x\n", fw_rev);
    }
    else
    {
        printk("bcm2835 firmware property channel not responding\n");
    }

    g_machine_power_off_func = &bcm2835_firmware_power_off;

    return MKDEV(BCM2835_FIRMWARE_MAJOR, 0);
}

bool bcm2835_firmware_property_call(void *buffer, size_t buffer_size)
{
    if (!g_bcm2835_firmware.isInitialized || buffer == NULL ||
        (buffer_size & 0xF) != 0 || (((size_t)buffer) & 0xF) != 0)
    {
        return false;
    }

    uint32_t *msg = (uint32_t *)buffer;
    if (msg[0] != (uint32_t)buffer_size)
    {
        return false;
    }

    uint32_t pa = (uint32_t)virt_to_phys((size_t)buffer);
    uint32_t resp = 0;

    spin_lock(&g_bcm2835_firmware.lock);
    bool ok = bcm2835_mbox_call(BCM2835_MBOX_CHANNEL_PROPERTY_TAGS, pa, &resp);
    spin_unlock(&g_bcm2835_firmware.lock);

    if (!ok)
    {
        return false;
    }

    return ((msg[1] & RPI_FIRMWARE_STATUS_SUCCESS) != 0);
}

bool bcm2835_firmware_get_revision(uint32_t *revision_out)
{
    // message size and tag framing follow the mailbox property interface
    static uint32_t __attribute__((aligned(16))) msg[] = {
        7 * sizeof(uint32_t),
        0,
        RPI_FIRMWARE_TAG_GET_FIRMWARE_REVISION,
        4,
        0,
        0,
        0,
    };

    if (!g_bcm2835_firmware.isInitialized)
    {
        return false;
    }

    // reset mutable fields before every call
    msg[1] = 0;
    msg[5] = 0;

    if (!bcm2835_firmware_property_call(msg, sizeof(msg)))
    {
        return false;
    }

    if (revision_out != NULL)
    {
        *revision_out = msg[5];
    }

    return true;
}

bool bcm2835_firmware_get_clock_rate(uint32_t clock_id, uint32_t *rate_out)
{
    // mailbox property buffer: header + tag + end tag
    static uint32_t __attribute__((aligned(16))) msg[] = {
        8 * sizeof(uint32_t), 0, RPI_FIRMWARE_TAG_GET_CLOCK_RATE, 8, 0, 0, 0, 0,
    };

    if (!g_bcm2835_firmware.isInitialized)
    {
        return false;
    }

    // reset mutable fields before every call
    msg[1] = 0;
    msg[4] = 0;
    msg[5] = clock_id;
    msg[6] = 0;

    if (!bcm2835_firmware_property_call(msg, sizeof(msg)))
    {
        return false;
    }

    if (rate_out != NULL)
    {
        *rate_out = msg[6];
    }

    return true;
}

void bcm2835_firmware_power_off()
{
    // The BCM2835 firmware doesn't have a property call for power off, but
    // instead we can trigger a shutdown by setting the power state of the
    // SD card to "off" and waiting for the firmware to power off the machine.
    // This is a bit hacky but seems to be the recommended way to trigger a
    // shutdown via the BCM2835 firmware.
    static uint32_t __attribute__((aligned(16))) msg[] = {
        8 * sizeof(uint32_t),
        0,
        RPI_FIRMWARE_TAG_SET_POWER_STATE,
        8,
        0,
        RPI_FIRMWARE_DEVICE_ID_SD_CARD,
        RPI_FIRMWARE_SET_POWER_STATE_WAIT,
        0,
    };

    // reset mutable fields before each call
    msg[1] = 0;
    msg[4] = 0;

    if (!bcm2835_firmware_property_call(msg, sizeof(msg)))
    {
        printk("bcm2835 firmware power-off command failed\n");
    }
    else
    {
        printk("bcm2835 firmware power-off command accepted\n");
    }

    size_t t0 = seconds_since_boot();
    while (seconds_since_boot() - t0 < 3)
    {  // wait up to 3 seconds for the machine to power off
       // on qemu this will not power off the machine, but on real hardware it
       // should.
    }

    // qemu fallback: if qemu is started with -no-reboot, it will
    // also trigger a halt / qemu quit.
    g_machine_restart_func();

    // wait for power off (should happen within a few seconds)
    infinite_loop;
}
