/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

void plic_init();
void plic_init_per_cpu();
int plic_claim();
void plic_complete(int);
