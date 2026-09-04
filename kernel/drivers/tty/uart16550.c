/* SPDX-License-Identifier: MIT */

//
// low-level driver routines for 16550a UART.
//

#include <arch/asm.h>
#include <drivers/driver.h>
#include <drivers/tty/console.h>
#include <drivers/tty/uart16550.h>
#include <init/system.h>
#include <kernel/cpu.h>
#include <kernel/pgtable.h>
#include <kernel/proc.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

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
#define ISR \
    2  //< interrupt status register: which interrupt occured, read only - also
       // called IIR (Interrupt Identification Register)
#define ISR_INT_NONE (1)             ///< No interrupt pending
#define ISR_INT_RX_STATUS (0x06)     ///<
#define ISR_INT_RX_DATA (0x04)       ///< Data is ready to read
#define ISR_INT_RX_TIMEOUT (0x0C)    ///< Data is ready and the FIFO is full!
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

REGISTER_DRIVER("ns16550a", uart_init);
REGISTER_DRIVER("snps,dw-apb-uart", uart_init);

atomic_size_t g_uart_next_minor = 0;

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

static inline uint8_t uart_get_interrupt_enable(struct uart_16550 *uart)
{
    return (uint8_t)read_register(uart, IER);
}

static inline void uart_set_interrupt_enable(struct uart_16550 *uart,
                                             uint8_t ier)
{
    write_register(uart, IER, ier);
}

static inline void uart_enable_tx_interrupt(struct uart_16550 *uart)
{
    uart_set_interrupt_enable(uart,
                              uart_get_interrupt_enable(uart) | IER_TX_ENABLE);
}

static inline void uart_disable_tx_interrupt(struct uart_16550 *uart)
{
    uart_set_interrupt_enable(uart,
                              uart_get_interrupt_enable(uart) & ~IER_TX_ENABLE);
}

dev_t uart_init(struct Device_Init_Parameters *init_parameters,
                const char *name)
{
    DRIVER_CHECK_INIT_PARAMS(init_parameters);
    DEBUG_EXTRA_ASSERT(init_parameters->reg_io_width == 1 ||
                           init_parameters->reg_io_width == 4,
                       "unsupported IO width");

    struct uart_16550 *uart =
        kmalloc(sizeof(struct uart_16550), ALLOC_FLAG_ZERO_MEMORY);
    if (uart == NULL)
    {
        return INVALID_DEVICE;
    }

    uart->mmio_base = init_parameters->mem[0].start_va;
    uart->reg_io_width = init_parameters->reg_io_width;
    uart->reg_shift = init_parameters->reg_shift;

    size_t minor = (size_t)atomic_fetch_add(&g_uart_next_minor, 1);
    const size_t NAME_LEN = 16;
    char *device_name = kmalloc(NAME_LEN, ALLOC_FLAG_NONE);
    if (device_name == NULL)
    {
        printk("uart: out of memory\n");
        kfree(uart);
        return INVALID_DEVICE;
    }
    snprintf(device_name, NAME_LEN, "uart16550_%zd", minor);

    //   disable interrupts.
    write_register(uart, IER, 0x00);

    uart_set_baud_rate(&uart->tty, BAUD_115200);

    // reset and enable FIFOs.
    write_register(uart, FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    // enable receive interrupt; TX interrupt gets enabled only while
    // transmit queue has pending bytes.
    uint32_t interrupt_enable = IER_RX_ENABLE;
    write_register(uart, IER, interrupt_enable);

    // init uart_16550 object
    spin_lock_init(&uart->uart_tx_lock, "uart");

    uart->tty.putc = uart_putc;
    uart->tty.putc_sync = uart_putc_sync;
    uart->tty.poll_callback = NULL;
    uart->tty.set_baud_rate = uart_set_baud_rate;

    uart->tty.console = console_init(&uart->tty);
    if (uart->tty.console == NULL)
    {
        kfree(uart);
        kfree(device_name);
        return INVALID_DEVICE;
    }

    dev_t dev_id = MKDEV(UART_16550_MAJOR, minor);

    // init device and register it in the system
    size_t interrupt_count = init_parameters->interrupt_count;
    if ((g_system.compatible == SYSTEM_RISCV_SPIKE) && (interrupt_count > 1))
    {
        // Spike emits "interrupts = <irq 4>" for its one-cell PLIC. The
        // second value is intended as an active-high flag, not another IRQ.
        interrupt_count = 1;
    }
    dev_init(&uart->tty.dev, OTHER, dev_id, device_name,
             init_parameters->interrupts, interrupt_count,
             uart_interrupt_handler);
    register_device(&uart->tty.dev);

    return dev_id;
}

syserr_t uart_set_baud_rate(struct TTY_Device *tty, enum UART_BAUD_RATE rate)
{
    struct uart_16550 *uart = uart_16550_from_tty(tty);

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
        default: return -EINVAL;
    }

    // special mode to set baud rate.
    write_register(uart, LCR, LCR_BAUD_LATCH);

    write_register(uart, DLL, DLL_value);
    write_register(uart, DLM, DLM_value);

    // leave set-baud mode,
    // and set word length to 8 bits, no parity.
    write_register(uart, LCR, LCR_EIGHT_BITS);

    return 0;
}

void uart_putc(struct TTY_Device *tty, int32_t c)
{
    struct uart_16550 *uart = uart_16550_from_tty(tty);

    spin_lock(&uart->uart_tx_lock);

    while (uart->uart_tx_w == uart->uart_tx_r + UART_TX_BUF_SIZE)
    {
        // buffer is full.
        // wait for uart_send_buffer() to open up space in the buffer.
        sleep(&uart->uart_tx_r, &uart->uart_tx_lock);
    }
    uart->uart_tx_buf[uart->uart_tx_w % UART_TX_BUF_SIZE] = c;
    uart->uart_tx_w += 1;

    // Keep TX-empty interrupts enabled while there is buffered output.
    uart_enable_tx_interrupt(uart);

    uart_send_buffer(uart);
    spin_unlock(&uart->uart_tx_lock);
}

void uart_putc_sync(struct TTY_Device *tty, int32_t c)
{
    struct uart_16550 *uart = uart_16550_from_tty(tty);

    // Serialize direct THR access with the buffered transmit path. The spin
    // lock also disables device interrupts on this CPU while it is held.
    spin_lock(&uart->uart_tx_lock);

    while ((read_register(uart, LSR) & LSR_TX_IDLE) == 0)
    {
        // wait for Transmit Holding Empty to be set in LSR.
        ARCH_ASM_NOP;
    }
    write_register(uart, THR, c);

    spin_unlock(&uart->uart_tx_lock);
}

/// @brief If the UART is idle, and a character is waiting
/// in the transmit buffer, send it.
/// Caller must hold uart_tx_lock.
/// Called from both the top- and bottom-half.
void uart_send_buffer(struct uart_16550 *uart)
{
    while (true)
    {
        if (uart->uart_tx_w == uart->uart_tx_r)
        {
            // transmit buffer is empty.
            uart_disable_tx_interrupt(uart);
            return;
        }

        if ((read_register(uart, LSR) & LSR_TX_IDLE) == 0)
        {
            // the UART transmit holding register is full,
            // so we cannot give it another byte.
            // it will interrupt when it's ready for a new byte.
            return;
        }

        int32_t c = uart->uart_tx_buf[uart->uart_tx_r % UART_TX_BUF_SIZE];
        uart->uart_tx_r += 1;

        // maybe uart_putc() is waiting for space in the buffer.
        wakeup(&uart->uart_tx_r);

        write_register(uart, THR, c);
    }
}

/// @brief Read a character from UART
/// @return The char on success or -1 on failure
int32_t uart_getc(struct uart_16550 *uart)
{
    if (read_register(uart, LSR) & LSR_DATA_READY)
    {
        // input data is ready.
        return read_register(uart, RHR);
    }
    else
    {
        return -1;
    }
}

void uart_handle_input(struct uart_16550 *uart)
{
    // read and process incoming characters.
    while (true)
    {
        int c = uart_getc(uart);
        if (c == -1)
        {
            break;
        }
        console_interrupt_handler(uart->tty.console, c);
    }
}

/// @brief Handle a uart interrupt, raised because input has
/// arrived, or the uart is ready for more output, or
/// both. called from interrupt_handler().
void uart_interrupt_handler(dev_t dev)
{
    struct Device *device = dev_by_device_number(dev);
    struct TTY_Device *tty = tty_device_from_device(device);
    struct uart_16550 *uart = uart_16550_from_tty(tty);

    bool interrupt_done = false;

    if (g_system.compatible == SYSTEM_RISCV_SPIKE)
    {
        // Spike does not model IIR acknowledgement and prioritization like a
        // real 16550. Inspect the actual line conditions once and then
        // complete the PLIC claim instead of dispatching on its IIR value.
        uint8_t line_status = read_register(uart, LSR);
        if (line_status & LSR_DATA_READY)
        {
            uart_handle_input(uart);
        }

        spin_lock(&uart->uart_tx_lock);
        if ((line_status & LSR_TX_IDLE) &&
            (uart_get_interrupt_enable(uart) & IER_TX_ENABLE))
        {
            uart_send_buffer(uart);
        }
        spin_unlock(&uart->uart_tx_lock);

        return;
    }

    // A UART may have another source pending by the time the current source
    // has been handled, so loop until all are handled.
    while (interrupt_done == false)
    {
        uint8_t interrupt = read_register(uart, ISR) & 0x0F;
        if (interrupt & ISR_INT_NONE)
        {
            interrupt_done = true;
            break;
        }

        switch (interrupt)
        {
            case ISR_INT_RX_DATA: uart_handle_input(uart); break;
            case ISR_INT_TX_EMPTY:
                spin_lock(&uart->uart_tx_lock);
                uart_send_buffer(uart);
                spin_unlock(&uart->uart_tx_lock);
                break;
            case ISR_INT_MODEM_STATUS:
                // Reading MSR acknowledges a modem-status interrupt.
                read_register(uart, MSR);
                break;
            case ISR_INT_DMA_RX_END:
            case ISR_INT_DMA_TX_END:
                // Unsupported device-specific DMA
                interrupt_done = true;
                break;
            case ISR_INT_RX_STATUS:
            {
                // Reading LSR acknowledges a receiver-line-status interrupt.
                read_register(uart, LSR);
                break;
            }
            case ISR_INT_RX_TIMEOUT: uart_handle_input(uart); break;
            default:
            {
                printk("16550 UART: unknown interrupt %d\n", interrupt);
                interrupt_done = true;
                break;
            }
        }
    }
}
