/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

int memcmp(const void*, const void*, uint32_t);
void* memmove(void*, const void*, uint32_t);
void* memset(void*, int, uint32_t);
char* safestrcpy(char*, const char*, int);
int strlen(const char*);
int strncmp(const char*, const char*, uint32_t);
char* strncpy(char*, const char*, int);
