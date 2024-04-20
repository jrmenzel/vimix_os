/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

extern uint ticks;
void trapinit(void);
void trapinithart(void);
extern struct spinlock tickslock;
void usertrapret(void);
