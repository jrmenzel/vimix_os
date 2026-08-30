/* SPDX-License-Identifier: MIT */
#pragma once

//
// On RISC-V, the SBI (Supervisor Binary Interface) is used to call firmware
// functions, e.g. to power off the machine or start additional harts.
//
// An SBI firmware is not required for a RISC-V system, but required by VIMIX.
// (Targets without SBI can be compiled with a minimal SBI shim in m-mode).
//

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
/// @param hartid Hart ID of the hart to boot
/// @param opaque E.g. used to pass the device tree
/// @param start_pa Physical address to start at
syserr_t sbi_start_hart(size_t hartid, size_t opaque, size_t start_pa);

/// @brief Check if a hart has started after calling sbi_start_hart
/// @param hartid Hart ID of the hart to check
/// @return true if the hart has started, false otherwise
bool sbi_did_hart_start(size_t hartid);

/// @brief Trigger a software interrupt in other CPUs.
/// @param mask 64-bit CPU mask (even on 32-bit systems)
void sbi_send_ipi(uint64_t mask);
