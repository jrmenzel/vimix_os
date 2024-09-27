/* SPDX-License-Identifier: MIT */

//
// low-level driver routines for 16550a UART.
//

#include "uart16550.h"

#include <drivers/console.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <mm/memlayout.h>

/// the UART control registers are memory-mapped
/// at address UART0. this macro returns the
/// address of one of the registers.
size_t uart_base = 0;
#define Reg(reg) ((volatile unsigned char *)(uart_base + (reg)))

/// the UART control registers.
/// some have different meanings for
/// read vs write.
/// see http://byterunner.com/16550.html
#define RHR 0  // receive holding register (for input bytes)
#define THR 0  // transmit holding register (for output bytes)
#define IER 1  // interrupt enable register
#define IER_RX_ENABLE (1 << 0)
#define IER_TX_ENABLE (1 << 1)
#define ISR 2  //< which interrupt occured, read only
#define FCR 2  //< FIFO control register, write only
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)  // clear the content of the two FIFOs
#define ISR 2                    // interrupt status register
#define LCR 3                    // line control register
#define LCR_EIGHT_BITS (3 << 0)
#define LCR_BAUD_LATCH (1 << 7)  // special mode to set baud rate
#define LSR 5                    // line status register
#define LSR_RX_READY (1 << 0)    // input is waiting to be read from RHR
#define LSR_TX_IDLE (1 << 5)     // THR can accept another character to send

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

struct uart_16550 g_uart_16550;
extern volatile bool g_kernel_panicked;  // from printk.c

void uartstart();

void uart_init(struct Device_Memory_Map *dev_map)
{
    uart_base = dev_map->mem_start;

    // disable interrupts.
    WriteReg(IER, 0x00);

    // special mode to set baud rate.
    WriteReg(LCR, LCR_BAUD_LATCH);

    // LSB for baud rate of 38.4K.
    WriteReg(0, 0x03);

    // MSB for baud rate of 38.4K.
    WriteReg(1, 0x00);

    // leave set-baud mode,
    // and set word length to 8 bits, no parity.
    WriteReg(LCR, LCR_EIGHT_BITS);

    // reset and enable FIFOs.
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    // enable receive interrupt. IER_TX_ENABLE would add an interrupt, when the
    // send buffer is empty, but we busy wait on it to clear while sending.
    WriteReg(IER, IER_RX_ENABLE);

    // init uart_16550 object
    spin_lock_init(&g_uart_16550.uart_tx_lock, "uart");
}

void uart_putc(int32_t c)
{
    spin_lock(&g_uart_16550.uart_tx_lock);

    if (g_kernel_panicked)
    {
        while (true)
        {
        }
    }
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

    if (g_kernel_panicked)
    {
        while (true)
        {
        }
    }

    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    {
        // wait for Transmit Holding Empty to be set in LSR.
    }
    WriteReg(THR, c);

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
            ReadReg(ISR);  // clears the interrupt source
            return;
        }

        if ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
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

        WriteReg(THR, c);
    }
}

/// @brief Read a character from UART
/// @return The char on success or -1 on failure
int32_t uart_getc()
{
    if (ReadReg(LSR) & 0x01)
    {
        // input data is ready.
        return ReadReg(RHR);
    }
    else
    {
        return -1;
    }
}

/// @brief Handle a uart interrupt, raised because input has
/// arrived, or the uart is ready for more output, or
/// both. called from interrupt_handler().
void uart_interrupt_handler()
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

    // send buffered characters.
    spin_lock(&g_uart_16550.uart_tx_lock);
    uartstart();
    spin_unlock(&g_uart_16550.uart_tx_lock);
}
