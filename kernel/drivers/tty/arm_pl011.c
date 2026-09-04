/* SPDX-License-Identifier: MIT */

#include <arch/asm.h>
#include <drivers/driver.h>
#include <drivers/tty/arm_pl011.h>
#include <drivers/tty/console.h>
#include <kernel/cpu.h>

REGISTER_DRIVER("arm,pl011", arm_pl011_init);

atomic_size_t g_arm_pl011_next_minor = 0;

#define CR_TXEN (1 << 8)
#define CR_RXEN (1 << 9)
#define CR_UARTEN (1 << 0)
#define LCR_FEN (1 << 4)
#define LCR_WLEN_8BIT (3 << 5)
#define FR_BUSY (1 << 3)
#define FR_RXFE (1 << 4)
#define FR_TXFF (1 << 5)

#define IMSC_RXIM (1 << 4)
#define IMSC_RTIM (1 << 6)

#define MIS_RXMIS (1 << 4)
#define MIS_RTMIS (1 << 6)

#define ICR_ALL (0x7FF)

void calculate_divisors(uint32_t *integer, uint32_t *fractional,
                        uint32_t baud_rate)
{
    const uint32_t PL011_DEFAULT_UART_CLOCK_HZ = 24000000U;
    // 64 * F_UARTCLK / (16 * B) = 4 * F_UARTCLK / B
    uint32_t base_clock = PL011_DEFAULT_UART_CLOCK_HZ;
    const uint32_t div = 4 * base_clock / baud_rate;

    *fractional = div & 0x3f;
    *integer = (div >> 6) & 0xffff;
}

// register offsets
// https://developer.arm.com/documentation/ddi0183/g/programmers-model/summary-of-registers

// Data Register
#define PL011_UART_DR (0x0000)
// Receive Status Register / Error Clear Register
#define PL011_UART_RSR_ECR (0x0004)
// Flag Register
#define PL011_UART_FR (0x0018)
// IrDA Low-Power Counter Register
#define PL011_UART_ILPR (0x0020)
// Integer Baud Rate Register
#define PL011_UART_IBRD (0x0024)
// Fractional Baud Rate Register
#define PL011_UART_FBRD (0x0028)
// Line Control Register
#define PL011_UART_LCRH (0x002C)
// Control Register
#define PL011_UART_CR (0x0030)
// Interrupt FIFO Level Select Register
#define PL011_UART_IFLS (0x0034)
// Interrupt Mask Set/Clear Register
#define PL011_UART_IMSC (0x0038)
// Raw Interrupt Status Register
#define PL011_UART_RIS (0x003C)
// Masked Interrupt Status Register
#define PL011_UART_MIS (0x0040)
// Interrupt Clear Register
#define PL011_UART_ICR (0x0044)
// DMA Control Register
#define PL011_UART_DMACR (0x0048)
#define PL011_Peripheral_ID0 (0x0FE0)
#define PL011_Peripheral_ID1 (0x0FE4)
#define PL011_Peripheral_ID2 (0x0FE8)
#define PL011_Peripheral_ID3 (0x0FEC)
#define PL011_PCell_ID0 (0x0FF0)
#define PL011_PCell_ID1 (0x0FF4)
#define PL011_PCell_ID2 (0x0FF8)
#define PL011_PCell_ID3 (0x0FFC)

syserr_t arm_pl011_set_baud_rate(struct TTY_Device *tty,
                                 enum UART_BAUD_RATE rate)
{
    struct Arm_pl011 *arm_pl011 = arm_pl011_from_tty(tty);

    uint32_t integer = 0;
    uint32_t fractional = 0;
    calculate_divisors(&integer, &fractional, tty_get_baud_value(rate));
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_IBRD, integer);
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_FBRD, fractional);

    return 0;
}

dev_t arm_pl011_init(struct Device_Init_Parameters *init_parameters,
                     const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);

    struct Arm_pl011 *arm_pl011 =
        kmalloc(sizeof(struct Arm_pl011), ALLOC_FLAG_ZERO_MEMORY);
    if (arm_pl011 == NULL)
    {
        return INVALID_DEVICE;
    }

    size_t minor = (size_t)atomic_fetch_add(&g_arm_pl011_next_minor, 1);
    const size_t NAME_LEN = 16;
    char *device_name = kmalloc(NAME_LEN, ALLOC_FLAG_NONE);
    if (device_name == NULL)
    {
        printk("uart: out of memory\n");
        kfree(arm_pl011);
        return INVALID_DEVICE;
    }
    snprintf(device_name, NAME_LEN, "arm_pl011_%zd", minor);

    arm_pl011->mmio_base = init_parameters->mem[0].start_va;
    spin_lock_init(&arm_pl011->arm_pl011_lock, "arm_pl011_lock");

    // Disable UART before changing baud/line config.
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_CR, 0);

    // Mask and clear all interrupts before reconfiguration.
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_IMSC, 0);
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_ICR, ICR_ALL);

    // Configure baud rate for the default QEMU virt PL011 clock.
    arm_pl011_set_baud_rate(&arm_pl011->tty, BAUD_115200);

    // 8N1 and FIFO enabled.
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_LCRH,
                       LCR_WLEN_8BIT | LCR_FEN);

    // Enable receive interrupts and RX/TX/UART.
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_IMSC,
                       IMSC_RXIM | IMSC_RTIM);
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_CR,
                       CR_UARTEN | CR_TXEN | CR_RXEN);

    arm_pl011->tty.putc = arm_pl011_putc;
    arm_pl011->tty.putc_sync = arm_pl011_putc;
    arm_pl011->tty.poll_callback = NULL;
    arm_pl011->tty.set_baud_rate = arm_pl011_set_baud_rate;

    arm_pl011->tty.console = console_init(&arm_pl011->tty);
    if (arm_pl011->tty.console == NULL)
    {
        kfree(arm_pl011);
        kfree(device_name);
        return INVALID_DEVICE;
    }

    dev_t dev_id = MKDEV(ARM_PL011_MAJOR, minor);

    // init device and register it in the system
    dev_init(&arm_pl011->tty.dev, OTHER, dev_id, device_name,
             init_parameters->interrupts, init_parameters->interrupt_count,
             arm_pl011_interrupt_handler);
    register_device(&arm_pl011->tty.dev);

    return dev_id;
}

void arm_pl011_interrupt_handler(dev_t dev)
{
    struct Device *device = dev_by_device_number(dev);
    struct TTY_Device *tty = tty_device_from_device(device);
    struct Arm_pl011 *arm_pl011 = arm_pl011_from_tty(tty);

    uint32_t mis = MMIO_READ_UINT_32(arm_pl011->mmio_base, PL011_UART_MIS);
    if ((mis & (MIS_RXMIS | MIS_RTMIS)) == 0)
    {
        if (mis != 0)
        {
            MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_ICR, mis);
        }
        return;
    }

    // Drain all available RX bytes in one interrupt.
    while ((MMIO_READ_UINT_32(arm_pl011->mmio_base, PL011_UART_FR) & FR_RXFE) ==
           0)
    {
        int32_t c =
            (int32_t)(MMIO_READ_UINT_32(arm_pl011->mmio_base, PL011_UART_DR) &
                      0xFF);
        console_interrupt_handler(tty->console, c);
    }

    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_ICR, mis);
}

void arm_pl011_putc(struct TTY_Device *tty, int32_t c)
{
    struct Arm_pl011 *arm_pl011 = arm_pl011_from_tty(tty);

    cpu_push_disable_device_interrupt_stack();

    while (MMIO_READ_UINT_32(arm_pl011->mmio_base, PL011_UART_FR) & FR_TXFF)
    {
        ARCH_ASM_NOP;
    }
    MMIO_WRITE_UINT_32(arm_pl011->mmio_base, PL011_UART_DR,
                       (uint32_t)(c & 0xFF));

    while (MMIO_READ_UINT_32(arm_pl011->mmio_base, PL011_UART_FR) & FR_BUSY)
    {
        ARCH_ASM_NOP;
    }

    cpu_pop_disable_device_interrupt_stack();
}

int arm_pl011_getc(struct Arm_pl011 *arm_pl011)
{
    if (MMIO_READ_UINT_32(arm_pl011->mmio_base, PL011_UART_FR) & FR_RXFE)
    {
        return -1;
    }

    return (int)(MMIO_READ_UINT_32(arm_pl011->mmio_base, PL011_UART_DR) & 0xFF);
}

void arm_pl011_poll_input(struct TTY_Device *tty)
{
    struct Arm_pl011 *arm_pl011 = arm_pl011_from_tty(tty);

    while (true)
    {
        int32_t c = arm_pl011_getc(arm_pl011);
        if (c < 0)
        {
            break;
        }

        console_interrupt_handler(tty->console, c);
    }
}
