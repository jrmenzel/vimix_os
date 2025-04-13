/* SPDX-License-Identifier: MIT */
#pragma once

// The SATP register sets the page table (pointer to first page of PTEs), the
// mode and the ASID.
#if defined(__ARCH_32BIT)
// Use RISC V's Sv32 page table scheme:
// +------+---------+----------------------+
// | MODE |  ASID   |         PPN          |
// +------+---------+----------------------+
// | 31   |  30–22  |        21–0          |
// +------+---------+----------------------+

#define SATP_MODE_SV32 (1L << 31)
#define SATP_MODE SATP_MODE_SV32

#define SATP_ASID_POS (22)
#define SATP_ASID_MAX (0x1FF)
#define SATP_PPN_MASK (0x003FFFFF)

#define MAKE_SATP(pagetable) (SATP_MODE_SV32 | (((uint32_t)pagetable) >> 12))
#else
// Use RISC V's Sv39 page table scheme:
// +------+----------------+----------------------------------+
// | MODE |     ASID       |              PPN                 |
// +------+----------------+----------------------------------+
// |63–60 |    59–44       |             43–0                 |
// +------+----------------+----------------------------------+
#define SATP_MODE_SV39 (8L << 60)
#define SATP_MODE_SV48 (9L << 60)
#define SATP_MODE SATP_MODE_SV39

#define SATP_ASID_MAX (0xFFFFUL)
#define SATP_ASID_POS (44UL)
#define SATP_PPN_MASK (0xFFFFFFFFFFFUL)

#define MAKE_SATP(pagetable) (SATP_MODE_SV39 | (((uint64_t)pagetable) >> 12))
#endif  // __ARCH_32BIT

#define SATP_ASID_MASK (SATP_ASID_MAX << SATP_ASID_POS)
