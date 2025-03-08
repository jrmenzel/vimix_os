/* SPDX-License-Identifier: MIT */

//
// low-level driver routines for 16550a UART.
//

#include <arch/asm.h>
#include <drivers/console.h>
#include <drivers/mmio_access.h>
#include <drivers/uart16550.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/major.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <mm/memlayout.h>

/// the UART control registers.
/// some have different meanings for
/// read vs write.
/// see http://byterunner.com/16550.html
#define RHR 0                   //< receive holding register (for input bytes)
#define THR 0                   //< transmit holding register (for output bytes)
#define IER 1                   //< interrupt enable register
#define IER_RX_ENABLE (1 << 0)  //< data ready interrupt
#define IER_TX_ENABLE (1 << 1)  //< THR empty interrupt
#define IER_RLS_ENABLE (1 << 2)  //< Receiver line status interrupt
#define IER_MS_ENABLE (1 << 3)   //< Modem status interrupt
#define ISR 2  //< interrupt status register: which interrupt occured, read only
#define ISR_INT_NONE (1)             ///< No interrrupt pending
#define ISR_INT_RX_STATUS (0x06)     ///<
#define ISR_INT_RX_DATA (0x04)       ///< Data is ready to read
#define ISR_INT_RX_TIMEOUT (0x0C)    ///<
#define ISR_INT_TX_EMPTY (0x02)      ///<
#define ISR_INT_MODEM_STATUS (0x00)  ///<
#define ISR_INT_DMA_RX_END (0x0E)    ///<
#define ISR_INT_DMA_TX_END (0x0A)    ///<
#define FCR 2                        //< FIFO control register, write only
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)  //< clear the content of the two FIFOs
#define LCR 3                    //< line control register
#define MCR 4                    //< modem control register
#define LCR_EIGHT_BITS (3 << 0)
#define LCR_BAUD_LATCH (1 << 7)  //< set DLAB bit, special mode to set baud rate
#define LSR 5                    //< line status register
#define LSR_DATA_READY (1 << 0)  //< input is waiting to be read from RHR
#define LSR_TX_IDLE (1 << 5)     //< THR can accept another character to send
#define MSR 6                    //< modem status register

// bautrate divisor least significant byte; only visibly if DLAB bit is set
#define DLL 0

// bautrate divisor most significant byte; only visibly if DLAB bit is set
#define DLM 1

// baudrate prescaler devision
#define PSD 5

int32_t read_register(struct uart_16550 *uart, size_t reg)
{
    if (uart->reg_io_width == 1)
    {
        return MMIO_READ_UINT_8_SHIFT(uart->mmio_base, reg, uart->reg_shift);
    }
    else  // uart->reg_io_width == 4, only two supported options
    {
        return MMIO_READ_UINT_32_SHIFT(uart->mmio_base, reg, uart->reg_shift);
    }
}

void write_register(struct uart_16550 *uart, size_t reg, uint32_t value)
{
    if (uart->reg_io_width == 1)
    {
        MMIO_WRITE_UINT_8_SHIFT(uart->mmio_base, reg, uart->reg_shift, value);
    }
    else  // uart->reg_io_width == 4, only two supported options
    {
        MMIO_WRITE_UINT_32_SHIFT(uart->mmio_base, reg, uart->reg_shift, value);
    }
}

struct uart_16550 g_uart_16550;
bool g_uart_16550_initialized = false;

void uartstart();

dev_t uart_init(struct Device_Init_Parameters *init_param, const char *name)
{
    DEBUG_EXTRA_ASSERT(
        init_param->reg_io_width == 1 || init_param->reg_io_width == 4,
        "unsupported IO width");

    if (g_uart_16550_initialized)
        return INVALID_DEVICE;  // only one instance for now

    g_uart_16550.mmio_base = init_param->mem[0].start;
    g_uart_16550.reg_io_width = init_param->reg_io_width;
    g_uart_16550.reg_shift = init_param->reg_shift;

    //   disable interrupts.
    write_register(&g_uart_16550, IER, 0x00);

#ifndef __PLATFORM_VISIONFIVE2
    uart_set_baud_rate(BAUD_115200);
#endif

    // reset and enable FIFOs.
    write_register(&g_uart_16550, FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    //  enable receive/send interrupts
#ifdef __PLATFORM_SPIKE
    // somehow TX interrupts break Vimix on Spike: the interrupt does not get
    // cleared
    write_register(&g_uart_16550, IER, IER_RX_ENABLE);
#else
    write_register(&g_uart_16550, IER, IER_RX_ENABLE | IER_TX_ENABLE);
#endif

    // init uart_16550 object
    spin_lock_init(&g_uart_16550.uart_tx_lock, "uart");

    g_uart_16550_initialized = true;
    return MKDEV(UART_16550_MAJOR, 0);
}

bool uart_set_baud_rate(enum UART_BAUD_RATE rate)
{
    // the BAUD rate is the system clock / 16 / [DLM DLL] / (PSD+1)
    // the PSD register is not present in all 16650 UARTS
    // values below should work for 1.8432 MHz
    uint8_t DLM_value = 0;  // 0 for all supported BAUD rates
    uint8_t DLL_value = 0;

    switch (rate)
    {
        case BAUD_1200: DLL_value = 0x60; break;
        case BAUD_2400: DLL_value = 0x30; break;
        case BAUD_4800: DLL_value = 0x18; break;
        case BAUD_9600: DLL_value = 0x0C; break;
        case BAUD_19200: DLL_value = 0x06; break;
        case BAUD_38400: DLL_value = 0x03; break;
        case BAUD_57600: DLL_value = 0x02; break;
        case BAUD_115200: DLL_value = 0x01; break;
        default: return false;
    }

    // special mode to set baud rate.
    write_register(&g_uart_16550, LCR, LCR_BAUD_LATCH);

    write_register(&g_uart_16550, DLL, DLL_value);
    write_register(&g_uart_16550, DLM, DLM_value);

    // leave set-baud mode,
    // and set word length to 8 bits, no parity.
    write_register(&g_uart_16550, LCR, LCR_EIGHT_BITS);

    return true;
}

void uart_putc(int32_t c)
{
    spin_lock(&g_uart_16550.uart_tx_lock);

    while (g_uart_16550.uart_tx_w == g_uart_16550.uart_tx_r + UART_TX_BUF_SIZE)
    {
        // buffer is full.
        // wait for uartstart() to open up space in the buffer.
        sleep(&g_uart_16550.uart_tx_r, &g_uart_16550.uart_tx_lock);
    }
    g_uart_16550.uart_tx_buf[g_uart_16550.uart_tx_w % UART_TX_BUF_SIZE] = c;
    g_uart_16550.uart_tx_w += 1;
    uartstart();
    spin_unlock(&g_uart_16550.uart_tx_lock);
}

void uart_putc_sync(int32_t c)
{
    cpu_push_disable_device_interrupt_stack();

    while ((read_register(&g_uart_16550, LSR) & LSR_TX_IDLE) == 0)
    {
        // wait for Transmit Holding Empty to be set in LSR.
        ARCH_ASM_NOP;
    }
    write_register(&g_uart_16550, THR, c);

    cpu_pop_disable_device_interrupt_stack();
}

/// @brief If the UART is idle, and a character is waiting
/// in the transmit buffer, send it.
/// Caller must hold uart_tx_lock.
/// Called from both the top- and bottom-half.
void uartstart()
{
    while (true)
    {
        if (g_uart_16550.uart_tx_w == g_uart_16550.uart_tx_r)
        {
            // transmit buffer is empty.
            return;
        }

        if ((read_register(&g_uart_16550, LSR) & LSR_TX_IDLE) == 0)
        {
            // the UART transmit holding register is full,
            // so we cannot give it another byte.
            // it will interrupt when it's ready for a new byte.
            return;
        }

        int32_t c =
            g_uart_16550.uart_tx_buf[g_uart_16550.uart_tx_r % UART_TX_BUF_SIZE];
        g_uart_16550.uart_tx_r += 1;

        // maybe uart_putc() is waiting for space in the buffer.
        wakeup(&g_uart_16550.uart_tx_r);

        write_register(&g_uart_16550, THR, c);
    }
}

/// @brief Read a character from UART
/// @return The char on success or -1 on failure
int32_t uart_getc()
{
    if (read_register(&g_uart_16550, LSR) & LSR_DATA_READY)
    {
        // input data is ready.
        return read_register(&g_uart_16550, RHR);
    }
    else
    {
        return -1;
    }
}

/// @brief Handle a uart interrupt, raised because input has
/// arrived, or the uart is ready for more output, or
/// both. called from interrupt_handler().
void uart_interrupt_handler(dev_t dev)
{
    spin_lock(&g_uart_16550.uart_tx_lock);
    // reading the register clears the interrupt source
    uint8_t int_status = read_register(&g_uart_16550, ISR);
    uint8_t interrupt = int_status & 0x0F;

    switch (interrupt)
    {
        case ISR_INT_NONE: break;  // panic("no int pending");
        case ISR_INT_RX_DATA:
        {
            // read and process incoming characters.
            while (true)
            {
                int c = uart_getc();
                if (c == -1)
                {
                    break;
                }
                console_interrupt_handler(c);
            }
        }
        break;
        case ISR_INT_TX_EMPTY: uartstart(); break;
        case ISR_INT_MODEM_STATUS: break;
        case ISR_INT_DMA_RX_END: break;
        case ISR_INT_DMA_TX_END: break;
        case ISR_INT_RX_STATUS: break;
        default: panic("unknown interrupt");
    }
    uartstart();  // in case no TX empty interrupts are coming, currently on
                  // Spike
    spin_unlock(&g_uart_16550.uart_tx_lock);
}
