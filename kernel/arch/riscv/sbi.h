/* SPDX-License-Identifier: MIT */
#pragma once
#ifdef CONFIG_RISCV_SBI

#include <kernel/kernel.h>

/// @brief An SBI call returns an error value in a0 (packed into
/// error here) and a value in a1.
/// Taken directly from the SBI spec:
/// https://www.scs.stanford.edu/~zyedidia/docs/riscv/riscv-sbi.pdf
struct sbiret
{
    long error;
    long value;
};

/// @brief Looks for required SBI extensions, starts additional harts.
void init_sbi();

// legacy console
#define SBI_LEGACY_EXT_CONSOLE_PUTCHAR 0x01
#define SBI_LEGACY_EXT_CONSOLE_GETCHAR 0x02

/// @brief Legacy SBI debug console. Can block if reader is too slow.
/// @param ch char to write
void sbi_console_putchar(int ch);

/// @brief Legacy SBI debug console.
/// @return char or -1 on error.
long sbi_console_getchar();

/// @brief Call in regular intervals: the SBI console does not
/// trigger interrupts when data is ready.
void sbi_console_poll_input();

// SBI Base Extension
#define SBI_EXT_ID_BASE 0x10
#define SBI_BASE_GET_SPEC_VERSION 0
#define SBI_BASE_GET_IMPL_ID 1
#define SBI_BASE_GET_IMPL_VERSION 2
#define SBI_BASE_PROBE_EXTENSION 3
#define SBI_BASE_GET_MVENDORID 4
#define SBI_BASE_GET_MARCHID 5
#define SBI_BASE_GET_MIMPID 6

// Timer (TIME) Extension
#define SBI_EXT_ID_TIME 0x54494D45
#define SBI_TIME_SET_TIMER 0

/// @brief Sets the per CPU timer to trigger an interrupt.
void sbi_set_timer(uint64_t stime_value);

// S-mode IPI (IPI) Extension
#define SBI_EXT_ID_IPI 0x735049
#define SBI_IPI_SEND_IPI 0

// RFENCE (RFNC) Extension
#define SBI_EXT_ID_RFNC 0x52464E43
#define SBI_RFNC_REMOTE_FENCE_I 0
#define SBI_RFNC_REMOTE_SFENCE_VMA 1
#define SBI_RFNC_REMOTE_SFENCE_VMA_ASID 2
#define SBI_RFNC_REMOTE_HFENCE_GVMA_VMID 3
#define SBI_RFNC_REMOTE_HFENCE_GVMA 4
#define SBI_RFNC_REMOTE_HFENCE_VVMA_ASID 5
#define SBI_RFNC_REMOTE_HFENCE_VVMA 6

// Hart State Management (HSM) Extension
#define SBI_EXT_ID_HSM 0x48534D
#define SBI_HSM_HART_START 0
#define SBI_HSM_HART_STOP 1
#define SBI_HSM_HART_STATUS 2
#define SBI_HSM_HART_SUSPEND 3

// System Reset (SRST) Extension
#define SBI_EXT_ID_SRST 0x53525354
#define SBI_SRST_SYSTEM_RESET 0
#define SBI_SRST_TYPE_SHUTDOWN 0
#define SBI_SRST_TYPE_COLD_REBOOT 1
#define SBI_SRST_TYPE_WARM_REBOOT 2

#define SBI_SRST_REASON_NONE 0
#define SBI_SRST_REASON_SYSTEM_FAILURE 1

/// @brief Reboots or shuts down the system.
/// @param reset_type SBI_SRST_TYPE_*
/// @param reset_reason Optional SBI_SRST_REASON_*
/// Should not return
void sbi_system_reset(uint32_t reset_type, uint32_t reset_reason);

// Performance Monitoring Unit (PMU) Extension
#define SBI_EXT_ID_PMU 0x504D55
#define SBI_PMU_NUM_COUNTERS 0
#define SBI_PMU_COUNTER_GET_INFO 1
#define SBI_PMU_COUNTER_CONFIG_MATCHING 2
#define SBI_PMU_COUNTER_START 3
#define SBI_PMU_COUNTER_STOP 4
#define SBI_PMU_COUNTER_FW_READ 5

// SBI return error codes
#define SBI_SUCCESS 0
#define SBI_ERR_FAILED -1
#define SBI_ERR_NOT_SUPPORTED -2
#define SBI_ERR_INVALID_PARAM -3
#define SBI_ERR_DENIED -4
#define SBI_ERR_INVALID_ADDRESS -5
#define SBI_ERR_ALREADY_AVAILABLE -6
#define SBI_ERR_ALREADY_STARTED -7
#define SBI_ERR_ALREADY_STOPPED -8

#define SBI_SPEC_VERSION_MAJOR_SHIFT 24
#define SBI_SPEC_VERSION_MAJOR_MASK 0x7f
#define SBI_SPEC_VERSION_MINOR_MASK 0xffffff

/// @brief Tests if a SBI extension is available.
/// See https://www.scs.stanford.edu/~zyedidia/docs/riscv/riscv-sbi.pdf
/// @param extid Extension ID from the SBI spec
/// @return 1 if the extension is available, 0 otherwise
long sbi_probe_extension(int32_t extid);

#endif  // CONFIG_RISCV_SBI
