/* SPDX-License-Identifier: MIT */

#pragma once

#include "clint.h"
#include "plic.h"
#include "sbi.h"

/// init once from one CPU
static inline void init_interrupt_controller() { plic_init(); }

/// init once per CPU after one CPU called init_interrupt_controller()
static inline void init_interrupt_controller_per_hart() { plic_init_per_cpu(); }
