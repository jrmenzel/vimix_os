/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

void argint(int, int*);
int argstr(int, char*, int);
void argaddr(int, uint64_t*);
int fetchstr(uint64_t, char*, int);
int fetchaddr(uint64_t, uint64_t*);
void syscall();
