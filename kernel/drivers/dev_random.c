/* SPDX-License-Identifier: MIT */

#include <drivers/character_device.h>
#include <drivers/dev_random.h>
#include <drivers/rtc.h>
#include <kernel/major.h>
#include <kernel/proc.h>

struct
{
    struct Character_Device cdev;  ///< derived from a character device
    unsigned long rand_next;
} g_dev_random;

// from FreeBSD.
int do_rand(unsigned long *ctx)
{
    /*
     * Compute x = (7^5 * x) mod (2^31 - 1)
     * without overflowing 31 bits:
     *      (2^31 - 1) = 127773 * (7^5) + 2836
     * From "Random number generators: good ones are hard to find",
     * Park and Miller, Communications of the ACM, vol. 31, no. 10,
     * October 1988, p. 1195.
     */
    long hi, lo, x;

    /* Transform to [1, 0x7ffffffe] range. */
    x = (*ctx % 0x7ffffffe) + 1;
    hi = x / 127773;
    lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0) x += 0x7fffffff;
    /* Transform to [0, 0x7ffffffd] range. */
    x--;
    *ctx = x;
    return (x);
}

int rand() { return (do_rand(&g_dev_random.rand_next)); }

ssize_t dev_random_read(struct Device *dev, bool addr_is_userspace, size_t addr,
                        size_t len, uint32_t unused_file_offset)
{
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char value = rand() / (0x7ffffffd >> 8);
        int32_t ret = either_copyout(addr_is_userspace, addr + i, &value, 1);
        if (ret == -1) return -1;
    }

    return len;
}

dev_t dev_random_init(struct Device_Init_Parameters *param, const char *name)
{
    // init device and register it in the system
    g_dev_random.rand_next = rtc_get_time();
    g_dev_random.cdev.dev.name = "random";
    g_dev_random.cdev.dev.type = CHAR;
    g_dev_random.cdev.dev.device_number = MKDEV(DEV_RANDOM_MAJOR, 0);
    g_dev_random.cdev.ops.read = dev_random_read;
    g_dev_random.cdev.ops.write = character_device_write_unsupported;
    g_dev_random.cdev.ops.ioctl = NULL;
    dev_set_irq(&g_dev_random.cdev.dev, INVALID_IRQ_NUMBER, NULL);
    register_device(&g_dev_random.cdev.dev);

    return g_dev_random.cdev.dev.device_number;
}