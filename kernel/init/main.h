/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

///
/// first function called in supervisor mode for each hart
void main(void *dtb, bool is_boot_hart) __attribute__((noreturn));
