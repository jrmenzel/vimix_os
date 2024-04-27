/* SPDX-License-Identifier: MIT */

//
// low-level driver routines for 16550a UART.
//

#include "uart16550.h"

#include <kernel/console.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <mm/memlayout.h>

/// the UART control registers are memory-mapped
/// at address UART0. this macro returns the
/// address of one of the registers.
#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))

/// the UART control registers.
/// some have different meanings for
/// read vs write.
/// see http://byterunner.com/16550.html
#define RHR 0  // receive holding register (for input bytes)
#define THR 0  // transmit holding register (for output bytes)
#define IER 1  // interrupt enable register
#define IER_RX_ENABLE (1 << 0)
#define IER_TX_ENABLE (1 << 1)
#define FCR 2  // FIFO control register
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

/// the transmit output buffer.
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64_t uart_tx_w;  // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64_t uart_tx_r;  // read next from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]

extern volatile bool g_kernel_panicked;  // from printk.c

void uartstart();

void uart_init()
{
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

    // enable transmit and receive interrupts.
    WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

    // init uart_16550 object
    spin_lock_init(&uart_tx_lock, "uart");
}

void uart_putc(int32_t c)
{
    spin_lock(&uart_tx_lock);

    if (g_kernel_panicked)
    {
        while (true)
        {
        }
    }
    while (uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE)
    {
        // buffer is full.
        // wait for uartstart() to open up space in the buffer.
        sleep(&uart_tx_r, &uart_tx_lock);
    }
    uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
    uart_tx_w += 1;
    uartstart();
    spin_unlock(&uart_tx_lock);
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

    // wait for Transmit Holding Empty to be set in LSR.
    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
        ;
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
        if (uart_tx_w == uart_tx_r)
        {
            // transmit buffer is empty.
            return;
        }

        if ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
        {
            // the UART transmit holding register is full,
            // so we cannot give it another byte.
            // it will interrupt when it's ready for a new byte.
            return;
        }

        int32_t c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
        uart_tx_r += 1;

        // maybe uart_putc() is waiting for space in the buffer.
        wakeup(&uart_tx_r);

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
        if (c == -1) break;
        console_interrupt_handler(c);
    }

    // send buffered characters.
    spin_lock(&uart_tx_lock);
    uartstart();
    spin_unlock(&uart_tx_lock);
}
