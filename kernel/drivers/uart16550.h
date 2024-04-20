/* SPDX-License-Identifier: MIT */
#pragma once

void uartinit(void);
void uartintr(void);
void uartputc(int);
void uartputc_sync(int);
int uartgetc(void);
