/* SPDX-License-Identifier: MIT */
#pragma once

// instruction stream
#define isb() asm volatile("isb" : : : "memory")

// ordering only
#define dmb(opt) asm volatile("dmb " #opt : : : "memory")

// ordering and completion
#define dsb(opt) asm volatile("dsb " #opt : : : "memory")

// full system barrier
#define mb() dsb(sy)
#define rmb() dsb(ld)
#define wmb() dsb(st)

#define smp_mb() dsb(sy)
#define smp_rmb() dsb(ld)
#define smp_wmb() dsb(st)

#define dma_mb() dmb(osh)
#define dma_rmb() dmb(oshld)
#define dma_wmb() dmb(oshst)
