/* SPDX-License-Identifier: MIT */

#include <kernel/kernel.h>

// CPU Interface Control Register
#define GICC_CTLR 0x0000
// Interrupt Priority Mask Register
#define GICC_PMR 0x0004
// Binary Point Register
#define GICC_BPR 0x0008
// Interrupt Acknowledge Register
#define GICC_IAR 0x000c
// End of Interrupt Register
#define GICC_EOIR 0x0010
// Running Priority Register
#define GICC_RPR 0x0014
// Highest Priority Pending Interrupt Register
#define GICC_HPPIR 0x0018
// Aliased Interrupt Acknowledge Register[
#define GICC_AIAR 0x0020
// Aliased End of Interrupt Register
#define GICC_AEOIR 0x0024
// Aliased Highest Priority Pending Interrupt Register
#define GICC_AHPPIR 0x0028
// Active Priority Register
#define GICC_APR0 0x00d0
// Non-Secure Active Priority Register
#define GICC_NSAPR0 0x00e0
// CPU Interface Identification Register
#define GICC_IIDR 0x00fc
// Deactivate Interrupt Register
#define GICC_DIR 0x1000

#define GICD_CTLR 0x0000
#define GICD_TYPER 0x0004
#define GICD_IGROUPR(n) (0x0080 + (uint32_t)(n) * 4)
#define GICD_ISENABLER(n) (0x0100 + (uint32_t)(n) * 4)
#define GICD_ICENABLER(n) (0x0180 + (uint32_t)(n) * 4)
#define GICD_ISPENDR(n) (0x0200 + (uint32_t)(n) * 4)
#define GICD_ICPENDR(n) (0x0280 + (uint32_t)(n) * 4)
#define GICD_IPRIORITYR(n) (0x0400 + (uint32_t)(n) * 4)
#define GICD_ITARGETSR(n) (0x0800 + (uint32_t)(n) * 4)
#define GICD_ICFGR(n) (0x0c00 + (uint32_t)(n) * 4)
#define GICD_SGIR 0x0f00
