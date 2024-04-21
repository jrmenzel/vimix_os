/* SPDX-License-Identifier: MIT */
#pragma once

void uart_init();
void uart_interrupt_handler();
void uart_putc(int);
void uart_putc_sync(int);
int uart_getc();
