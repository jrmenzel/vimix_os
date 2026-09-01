/* SPDX-License-Identifier: MIT */

#include <arch/asm.h>
#include <drivers/bcm/bcm2835_aux.h>
#include <drivers/bcm/bcm2835_firmware.h>
#include <drivers/bcm/bcm2835_gpio.h>
#include <drivers/mmio_access.h>
#include <drivers/tty/bcm2835_aux_uart.h>
#include <drivers/tty/console.h>
#include <init/dtb.h>
#include <init/system.h>
#include <kernel/major.h>
#include <libfdt.h>

REGISTER_DRIVER("brcm,bcm2835-aux-uart", bcm2835_aux_uart_init);

// Auxilary mini UART registers
#define AUX_MU_IO (0x00)
#define AUX_MU_IER (0x04)
#define AUX_MU_IER_RX_ENABLE (0x01)
#define AUX_MU_IIR (0x08)
#define AUX_MU_IIR_R_FIFO (0x02)
#define AUX_MU_IIR_T_FIFO (0x04)
#define AUX_MU_IIR_FIFO_ENABLE (0xC0)
#define AUX_MU_LCR (0x0C)
#define AUX_MU_LCR_8BIT_MODE (0x03)
#define AUX_MU_MCR (0x10)
#define AUX_MU_LSR (0x14)
///< at least one byte is available
#define AUX_MU_LSR_DATA_READY (0x01)
///< set if the transmit FIFO can accept at leat one byte
#define AUX_MU_LSR_T_EMPTY (0x20)
///< set if the transmit FIFO is empty
#define AUX_MU_LSR_T_IDLE (0x40)
#define AUX_MU_MSR (0x18)
#define AUX_MU_SCRATCH (0x1C)
#define AUX_MU_CNTL (0x20)
#define AUX_MU_CNTL_R_ENABLE (1)
#define AUX_MU_CNTL_T_ENABLE (2)
#define AUX_MU_STAT (0x24)
#define AUX_MU_BAUD (0x28)

struct bcm2835_aux_uart g_bcm2835_aux_uart;

static int32_t bcm2835_aux_uart_getc_nonblocking()
{
    if (!(MMIO_READ_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_LSR) &
          AUX_MU_LSR_DATA_READY))
    {
        return -1;
    }

    return MMIO_READ_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_IO) & 0xFF;
}

void bcm2835_aux_uart_set_baud_rate(struct bcm2835_aux_uart *aux_uart,
                                    size_t baud_rate)
{
    uint32_t reg_value = ((aux_uart->clock_hz) / (baud_rate * 8)) - 1;
    MMIO_WRITE_UINT_32(aux_uart->mmio_base, AUX_MU_BAUD, reg_value);
}

static size_t bcm2835_aux_uart_get_clock(const void *dtb, int uart_node_offset)
{
    const size_t fallback = 500000000;

    if ((dtb == NULL) || (fdt_magic(dtb) != FDT_MAGIC)) return fallback;

    int node = uart_node_offset;
    if (node < 0)
    {
        node = fdt_node_offset_by_compatible(dtb, -1, "brcm,bcm2835-aux-uart");
    }
    if (node < 0) return fallback;

    // Some trees may provide the UART input clock directly on the UART node.
    size_t freq =
        dtb_read_prop_u32_with_fallback(dtb, node, "clock-frequency", 0);
    if (freq != 0) return freq;

    // Resolve the first clock provider from the UART node.
    int clocks_len = 0;
    const uint32_t *clocks = fdt_getprop(dtb, node, "clocks", &clocks_len);
    if ((clocks == NULL) || (clocks_len < (int)sizeof(uint32_t)))
        return fallback;

    int clock_node = fdt_node_offset_by_phandle(dtb, fdt32_to_cpu(clocks[0]));
    if (clock_node < 0) return fallback;

    // Some providers expose a direct clock-frequency.
    freq =
        dtb_read_prop_u32_with_fallback(dtb, clock_node, "clock-frequency", 0);
    if (freq != 0) return freq;

    // For Raspberry Pi this may chain UART -> AUX -> CPRMAN -> ...
    // so try one additional level.
    clocks = fdt_getprop(dtb, clock_node, "clocks", &clocks_len);
    if ((clocks == NULL) || (clocks_len < (int)sizeof(uint32_t)))
        return fallback;

    clock_node = fdt_node_offset_by_phandle(dtb, fdt32_to_cpu(clocks[0]));
    if (clock_node < 0) return fallback;

    freq =
        dtb_read_prop_u32_with_fallback(dtb, clock_node, "clock-frequency", 0);

    if (freq != 0) return freq;

    printk(
        "bcm2835_aux_uart: failed to find clock frequency in the device "
        "tree\n");
    return fallback;
}

dev_t bcm2835_aux_uart_init(struct Device_Init_Parameters *init_param,
                            const char *name)
{
    // this UART needs a GPIO provider to configure TX/RX pins
    struct Devices_List *dev_list = get_devices_list();
    bool gpio_init = init_device_by_name(dev_list, "brcm,bcm2835-gpio");
    gpio_init |= init_device_by_name(dev_list, "brcm,bcm2711-gpio");
    gpio_init |= init_device_by_name(dev_list, "brcm,bcm2835-gpiomem");
    if (!gpio_init) return INVALID_DEVICE;

    g_bcm2835_aux_uart.mmio_base = init_param->mem[0].start_va;

    size_t clock;
    if (getSystemCompatible() == SYSTEM_ARM64_RASPBERRY_PI_4)
    {
        // On Raspberry Pi 4 the mini-UART baud base follows the core clock.
        clock = 500000000;
        bool firmware_init =
            init_device_by_name(dev_list, "raspberrypi,bcm2835-firmware");
        uint32_t fw_clock_hz = 0;
        if (firmware_init &&
            bcm2835_firmware_get_clock_rate(BCM2835_FIRMWARE_CLOCK_ID_CORE,
                                            &fw_clock_hz) &&
            (fw_clock_hz != 0))
        {
            clock = (size_t)fw_clock_hz;
        }
        else
        {
            printk(
                "bcm2835_aux_uart: failed to query core clock from firmware; "
                "using fallback %zu Hz\n",
                clock);
        }
    }
    else
    {
        clock =
            bcm2835_aux_uart_get_clock(init_param->dtb, init_param->dev_offset);
    }
    g_bcm2835_aux_uart.clock_hz = clock;
    spin_lock_init(&g_bcm2835_aux_uart.lock, "bcm2835_aux_uart_lock");

    // We should read the clocks from the init_param and find the right
    // device that way. But at least on Raspberry Pi 3/4 this is the right one.
    // Also the clocks from the device tree will ensure it was initilized before
    // this UART.
    if (!bcm2835_aux_enable(BCM2835_AUX_DEVICE_UART)) return INVALID_DEVICE;

    MMIO_WRITE_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_CNTL, 0);
    MMIO_WRITE_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_MCR, 0);
    MMIO_WRITE_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_IER,
                       AUX_MU_IER_RX_ENABLE);
    MMIO_WRITE_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_LCR,
                       AUX_MU_LCR_8BIT_MODE);
    // clear transmit and recieve FIFOs
    MMIO_WRITE_UINT_32(
        g_bcm2835_aux_uart.mmio_base, AUX_MU_IIR,
        AUX_MU_IIR_FIFO_ENABLE | AUX_MU_IIR_T_FIFO | AUX_MU_IIR_R_FIFO);
    bcm2835_aux_uart_set_baud_rate(&g_bcm2835_aux_uart, 115200);

    // setup GPIO pins:
    bcm2835_gpio_set_pin_to_function(14, GPFSEL_FUNC_ALT_5);
    bcm2835_gpio_set_pull_up_control(14, GPPUD_OFF);
    bcm2835_gpio_set_pin_to_function(15, GPFSEL_FUNC_ALT_5);
    bcm2835_gpio_set_pull_up_control(15, GPPUD_OFF);

    // enable Rx & Tx
    MMIO_WRITE_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_CNTL,
                       AUX_MU_CNTL_R_ENABLE | AUX_MU_CNTL_T_ENABLE);

    return MKDEV(BCM2835_UART_AUX_MAJOR, 0);
}

void bcm2835_aux_uart_interrupt_handler(dev_t dev)
{
    (void)dev;

    // Drain all available RX bytes to avoid losing characters when the FIFO
    // already contains multiple bytes per interrupt.
    while (true)
    {
        int32_t c = bcm2835_aux_uart_getc_nonblocking();
        if (c < 0)
        {
            break;
        }

        console_interrupt_handler(c);
    }
}

void bcm2835_aux_uart_poll_input()
{
    while (true)
    {
        int32_t c = bcm2835_aux_uart_getc_nonblocking();
        if (c < 0)
        {
            break;
        }

        console_interrupt_handler(c);
    }
}

void bcm2835_aux_uart_putc(int32_t c) { bcm2835_aux_uart_putc_sync(c); }

void bcm2835_aux_uart_putc_sync(int32_t c)
{
    // wait until we can send
    do
    {
        ARCH_ASM_NOP;
    } while (!(MMIO_READ_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_LSR) &
               AUX_MU_LSR_T_EMPTY));

    // write the character to the buffer
    MMIO_WRITE_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_IO, c);
}

int bcm2835_aux_uart_getc()
{
    // wait until something is in the buffer
    do
    {
        ARCH_ASM_NOP;
    } while (!(MMIO_READ_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_LSR) &
               AUX_MU_LSR_DATA_READY));

    // read it and return
    return MMIO_READ_UINT_32(g_bcm2835_aux_uart.mmio_base, AUX_MU_IO) & 0xFF;
}
