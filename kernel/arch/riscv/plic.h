/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

void plicinit(void);
void plicinithart(void);
int plic_claim(void);
void plic_complete(int);
