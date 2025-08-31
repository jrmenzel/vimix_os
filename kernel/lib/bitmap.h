/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief A bitmap is an array of size_t long enough to hold the requested
/// number of bits.
/// The practical limit is that bitmap uses kmalloc() which is
/// limited to one page = 32k bits.
typedef size_t *bitmap_t;

#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_SIZET))
#define BIT_WORD(nr) ((nr) / BITS_PER_SIZET)

/// @brief Allocate a bitmap holding at least nbits bits. All nbits are
/// initially 0.
/// @param nbits Number of bits to manage.
/// @return NULL on error.
bitmap_t bitmap_alloc(unsigned int nbits);

/// @brief Frees the bitmap.
/// @param bitmap Pointer to bitmap allocated by bitmap_alloc().
void bitmap_free(const bitmap_t bitmap);

static inline void set_bit(size_t bit, bitmap_t bitmap)
{
    size_t mask = BIT_MASK(bit);
    size_t *p = bitmap + BIT_WORD(bit);

    *p |= mask;
}

static inline void clear_bit(size_t bit, bitmap_t bitmap)
{
    size_t mask = BIT_MASK(bit);
    size_t *p = bitmap + BIT_WORD(bit);

    *p &= ~mask;
}

static inline void change_bit(size_t bit, bitmap_t bitmap)
{
    size_t mask = BIT_MASK(bit);
    size_t *p = bitmap + BIT_WORD(bit);

    *p ^= mask;
}

static inline bool test_bit(size_t bit, bitmap_t bitmap)
{
    size_t mask = BIT_MASK(bit);
    size_t *p = bitmap + BIT_WORD(bit);

    return (*p & mask);
}

ssize_t find_first_bit_of_value(bitmap_t bitmap, size_t nbits, bool value);

static inline ssize_t find_first_zero_bit(bitmap_t bitmap, size_t nbits)
{
    return find_first_bit_of_value(bitmap, nbits, false);
}

static inline ssize_t find_first_bit(bitmap_t bitmap, size_t nbits)
{
    return find_first_bit_of_value(bitmap, nbits, true);
}
