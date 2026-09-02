/* SPDX-License-Identifier: MIT */

#include <arch/asm.h>
#include <drivers/driver.h>
#include <drivers/tty/arm_pl011.h>
#include <drivers/tty/console.h>
#include <kernel/cpu.h>

REGISTER_DRIVER("arm,pl011", arm_pl011_init);

struct arm_pl011 g_arm_pl011;
bool g_arm_pl011_initialized = false;

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

#define PL011_DEFAULT_UART_CLOCK_HZ 24000000U
#define PL011_DEFAULT_BAUDRATE 115200U

void calculate_divisors(uint32_t *integer, uint32_t *fractional)
{
    // 64 * F_UARTCLK / (16 * B) = 4 * F_UARTCLK / B
    uint32_t base_clock = PL011_DEFAULT_UART_CLOCK_HZ;
    uint32_t baudrate = PL011_DEFAULT_BAUDRATE;
    const uint32_t div = 4 * base_clock / baudrate;

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

static inline uint32_t pl011_read(size_t reg)
{
    return MMIO_READ_UINT_32(g_arm_pl011.uart_base, reg);
}

static inline void pl011_write(size_t reg, uint32_t value)
{
    MMIO_WRITE_UINT_32(g_arm_pl011.uart_base, reg, value);
}

dev_t arm_pl011_init(struct Device_Init_Parameters *init_parameters,
                     const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);

    if (g_arm_pl011_initialized)
        return INVALID_DEVICE;  // only one instance for now

    g_arm_pl011.uart_base = init_parameters->mem[0].start_va;
    spin_lock_init(&g_arm_pl011.arm_pl011_lock, "arm_pl011_lock");

    // Disable UART before changing baud/line config.
    pl011_write(PL011_UART_CR, 0);

    // Mask and clear all interrupts before reconfiguration.
    pl011_write(PL011_UART_IMSC, 0);
    pl011_write(PL011_UART_ICR, ICR_ALL);

    // Configure baud rate for the default QEMU virt PL011 clock.
    uint32_t integer = 0;
    uint32_t fractional = 0;
    calculate_divisors(&integer, &fractional);
    pl011_write(PL011_UART_IBRD, integer);
    pl011_write(PL011_UART_FBRD, fractional);

    // 8N1 and FIFO enabled.
    pl011_write(PL011_UART_LCRH, LCR_WLEN_8BIT | LCR_FEN);

    // Enable receive interrupts and RX/TX/UART.
    pl011_write(PL011_UART_IMSC, IMSC_RXIM | IMSC_RTIM);
    pl011_write(PL011_UART_CR, CR_UARTEN | CR_TXEN | CR_RXEN);

    g_arm_pl011.tty.putc = arm_pl011_putc;
    g_arm_pl011.tty.putc_sync = arm_pl011_putc;
    g_arm_pl011.tty.poll_callback = arm_pl011_poll_input;

    g_arm_pl011_initialized = true;

    return MKDEV(ARM_PL011_MAJOR, 0);
}

void arm_pl011_interrupt_handler(dev_t dev)
{
    (void)dev;

    uint32_t mis = pl011_read(PL011_UART_MIS);
    if ((mis & (MIS_RXMIS | MIS_RTMIS)) == 0)
    {
        if (mis != 0)
        {
            pl011_write(PL011_UART_ICR, mis);
        }
        return;
    }

    // Drain all available RX bytes in one interrupt.
    while ((pl011_read(PL011_UART_FR) & FR_RXFE) == 0)
    {
        int32_t c = (int32_t)(pl011_read(PL011_UART_DR) & 0xFF);
        console_interrupt_handler(c);
    }

    pl011_write(PL011_UART_ICR, mis);
}

void arm_pl011_putc(int32_t c)
{
    cpu_push_disable_device_interrupt_stack();

    while (pl011_read(PL011_UART_FR) & FR_TXFF)
    {
        ARCH_ASM_NOP;
    }
    pl011_write(PL011_UART_DR, (uint32_t)(c & 0xFF));

    while (pl011_read(PL011_UART_FR) & FR_BUSY)
    {
        ARCH_ASM_NOP;
    }

    cpu_pop_disable_device_interrupt_stack();
}

int arm_pl011_getc()
{
    if (pl011_read(PL011_UART_FR) & FR_RXFE)
    {
        return -1;
    }

    return (int)(pl011_read(PL011_UART_DR) & 0xFF);
}

void arm_pl011_poll_input()
{
    while (true)
    {
        int32_t c = arm_pl011_getc();
        if (c < 0)
        {
            break;
        }

        console_interrupt_handler(c);
    }
}
