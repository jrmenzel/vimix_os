/* SPDX-License-Identifier: MIT */

#include <drivers/tty/tty_device.h>
#include <kernel/errno.h>

syserr_t tty_set_baud_rate_unsupported(struct TTY_Device *tty,
                                       enum UART_BAUD_RATE rate)
{
    return -EOTHER;
}

uint32_t tty_get_baud_value(enum UART_BAUD_RATE baud)
{
    switch (baud)
    {
        case BAUD_1200: return 1200;
        case BAUD_2400: return 2400;
        case BAUD_4800: return 4800;
        case BAUD_9600: return 9600;
        case BAUD_19200: return 19200;
        case BAUD_38400: return 38400;
        case BAUD_57600: return 57600;
        case BAUD_115200: return 115200;
        default: panic("Unsupported BAUD rate");
    }
    return 0;
}
