/* SPDX-License-Identifier: MIT */

#include <kernel/kalloc.h>
#include <lib/bitmap.h>
#include <lib/minmax.h>

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

// returns how many size_t are needed to hold n bits.
#define BITS_TO_SIZET(n) DIV_ROUND_UP(n, (sizeof(size_t) * 8))

bitmap_t bitmap_alloc(unsigned int nbits)
{
    // allocate the minimal number of size_t
    return kmalloc(BITS_TO_SIZET(nbits) * sizeof(size_t));
}

void bitmap_free(const bitmap_t bitmap) { kfree((void *)bitmap); }

ssize_t find_first_bit_of_value(bitmap_t bitmap, size_t nbits, bool value)
{
    size_t nwords = BITS_TO_SIZET(nbits);
    size_t none_found_mask = value ? 0 : ~0;

    for (size_t i = 0; i < nwords; ++i)
    {
        size_t word = bitmap[i];

        if (word == none_found_mask)
        {
            nbits -= BITS_PER_SIZET;
            continue;
        }

        size_t bits_to_check = min(BITS_PER_SIZET, nbits);
        for (size_t bit = 0; bit < bits_to_check; ++bit)
        {
            if (test_bit(bit, &word) == value)
                return (bit + BITS_PER_SIZET * i);
        }
        nbits -= BITS_PER_SIZET;
    }

    return -1;
}
