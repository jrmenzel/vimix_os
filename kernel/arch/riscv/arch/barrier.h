/* SPDX-License-Identifier: MIT */
#pragma once

#define RISCV_FENCE_ASM(p, s) "\tfence " #p "," #s "\n"
#define RISCV_FENCE(p, s) \
    ({ __asm__ __volatile__(RISCV_FENCE_ASM(p, s) : : : "memory"); })

#define mb() RISCV_FENCE(iorw, iorw)
#define rmb() RISCV_FENCE(ir, ir)
#define wmb() RISCV_FENCE(ow, ow)

#define smp_mb() RISCV_FENCE(rw, rw)
#define smp_rmb() RISCV_FENCE(r, r)
#define smp_wmb() RISCV_FENCE(w, w)

#define dma_mb() RISCV_FENCE(iorw, iorw)
#define dma_rmb() RISCV_FENCE(ir, ir)
#define dma_wmb() RISCV_FENCE(ow, ow)
