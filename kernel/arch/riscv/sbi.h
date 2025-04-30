/* SPDX-License-Identifier: MIT */
#pragma once

#include <arch/riscv/sbi_defs.h>
#include <kernel/kernel.h>

/// @brief Looks for required SBI extensions, starts additional harts.
void init_sbi();

/// @brief Legacy SBI debug console. Can block if reader is too slow.
/// @param ch char to write
void sbi_console_putchar(int ch);

/// @brief Legacy SBI debug console.
/// @return char or -1 on error.
long sbi_console_getchar();

/// @brief Call in regular intervals: the SBI console does not
/// trigger interrupts when data is ready.
void sbi_console_poll_input();

/// @brief Sets the per CPU timer to trigger an interrupt.
void sbi_set_timer(uint64_t stime_value);

/// @brief Reboots or shuts down the system.
/// @param reset_type SBI_SRST_TYPE_*
/// @param reset_reason Optional SBI_SRST_REASON_*
/// Should not return
void sbi_system_reset(uint32_t reset_type, uint32_t reset_reason);

/// @brief Tests if a SBI extension is available.
/// See https://www.scs.stanford.edu/~zyedidia/docs/riscv/riscv-sbi.pdf
/// @param extid Extension ID from the SBI spec
/// @return 1 if the extension is available, 0 otherwise
long sbi_probe_extension(int32_t extid);

/// @brief Boots additional harts (other than boot hart) with the given
/// parameter
/// @param opaque E.g. used to pass the device tree
void sbi_start_harts(size_t opaque);
