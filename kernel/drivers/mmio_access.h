/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/stdint.h>

// clang-format off
#define _MMIO_READ(base, r, shift, TYPE) ((TYPE) *((volatile TYPE *)((base) + ((r) << (shift)))))

#define MMIO_READ_UINT_8_SHIFT(base, r, shift)  _MMIO_READ(base, r, shift, uint8_t)
#define MMIO_READ_UINT_32_SHIFT(base, r, shift) _MMIO_READ(base, r, shift, uint32_t)
#define MMIO_READ_UINT_64_SHIFT(base, r, shift) _MMIO_READ(base, r, shift, uint64_t)

#define MMIO_READ_UINT_8(base, r)  MMIO_READ_UINT_8_SHIFT(base,  r, 0)
#define MMIO_READ_UINT_32(base, r) MMIO_READ_UINT_32_SHIFT(base, r, 0)
#define MMIO_READ_UINT_64(base, r) MMIO_READ_UINT_64_SHIFT(base, r, 0)

#define _MMIO_WRITE(base, r, shift, value, TYPE) (*((volatile TYPE *)((base) + ((r) << (shift)))) = (TYPE) (value))

#define MMIO_WRITE_UINT_8_SHIFT(base,  r, shift, value) _MMIO_WRITE(base, r, shift, value, uint8_t)
#define MMIO_WRITE_UINT_32_SHIFT(base, r, shift, value) _MMIO_WRITE(base, r, shift, value, uint32_t)
#define MMIO_WRITE_UINT_64_SHIFT(base, r, shift, value) _MMIO_WRITE(base, r, shift, value, uint64_t)

#define MMIO_WRITE_UINT_8(base,  r, value) MMIO_WRITE_UINT_8_SHIFT(base,  r, 0, value)
#define MMIO_WRITE_UINT_32(base, r, value) MMIO_WRITE_UINT_32_SHIFT(base, r, 0, value)
#define MMIO_WRITE_UINT_64(base, r, value) MMIO_WRITE_UINT_64_SHIFT(base, r, 0, value)
// clang-format on
