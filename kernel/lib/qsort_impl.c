/// quicksort implementation based on Unix V7 algorithm (see
/// licenses/Caldera-license.pdf)
///
/// - Choose a middle element as pivot.
/// - Partition elements < pivot to the left, > pivot to the right.
/// - Handle elements equal to pivot by moving them next to the pivot
///   (three-way exchanges) to reduce swaps.
/// - Recurse on the smaller partition first to limit stack depth.
///
/// The implementation operates on bytes and swaps elements of size
/// `elem_size` using byte-wise moves so it works for arbitrary element sizes.
///
/// Changes:
/// - Moderenized C, removed global state

#include <kernel/stddef.h>
#include <kernel/stdlib.h>

/// @brief qs_partition - sort the range [lo, hi) (bytes) using elem_size and
/// cmp_func
/// @param lo pointer to first byte of the range
/// @param hi pointer one-past-the-last byte of the range
/// @details Partitions and recursively sorts the range. Internal routine
/// implementing the Unix V7 quicksort algorithm.
static void qs_partition(unsigned char *lo, unsigned char *hi, size_t elem_size,
                         compare_func compare);

/// @brief swap_bytes - swap two elements (elem_size bytes)
/// @param a pointer to first element
/// @param b pointer to second element
static void swap_bytes(unsigned char *a, unsigned char *b, size_t elem_size);

/// @brief rotate_three - rotate three elements: a <- c, b <- a, c <- b
/// @param a pointer to first element
/// @param b pointer to second element
/// @param c pointer to third element
/// @details Performs the three-way exchange
static void rotate_three(unsigned char *a, unsigned char *b, unsigned char *c,
                         size_t elem_size);

/// @brief qsort - see stdlib.h
void qsort(void *base, size_t nmemb, size_t size, compare_func compare)
{
    if (nmemb == 0 || size == 0 || base == NULL || compare == NULL) return;

    // call partition on [base, base + nmemb * size)
    qs_partition((unsigned char *)base, (unsigned char *)base + nmemb * size,
                 size, compare);
}

/// @brief Partition and recursively sort a byte-range
/// @param lo pointer to first byte of the range
/// @param hi pointer one-past-last byte of the range
static void qs_partition(unsigned char *lo, unsigned char *hi, size_t elem_size,
                         compare_func compare)
{
start:
    size_t range_bytes = (size_t)(hi - lo);
    if (range_bytes <= elem_size) return;  // zero or one element

    // choose middle element as pivot: compute an offset that's a multiple
    // of elem_size so pivot points to an element boundary
    size_t middle_offset = elem_size * (range_bytes / (2 * elem_size));
    unsigned char *pivot_lo = lo + middle_offset;
    unsigned char *pivot_hi = pivot_lo;

    unsigned char *left = lo;
    unsigned char *right = hi - elem_size;

    for (;;)
    {
        if (left < pivot_lo)
        {
            int cmp = (*compare)(left, pivot_lo);
            if (cmp == 0)
            {
                // equal: move element next to pivot on the left side
                pivot_lo -= elem_size;
                swap_bytes(left, pivot_lo, elem_size);
                continue;
            }
            if (cmp < 0)
            {
                left += elem_size;
                continue;
            }
        }

    loop_right:
        if (right > pivot_hi)
        {
            int cmp = (*compare)(pivot_hi, right);
            if (cmp == 0)
            {
                // equal: move element next to pivot on the right side
                pivot_hi += elem_size;
                swap_bytes(pivot_hi, right, elem_size);
                goto loop_right;
            }
            if (cmp > 0)
            {
                // pivot < right
                if (left == pivot_lo)
                {
                    // special case: do a three-way rotation to move pivot-range
                    pivot_hi += elem_size;
                    rotate_three(left, pivot_hi, right, elem_size);
                    left = pivot_lo += elem_size;
                    goto loop_right;
                }
                swap_bytes(left, right, elem_size);
                right -= elem_size;
                left += elem_size;
                continue;
            }
            right -= elem_size;
            goto loop_right;
        }

        if (left == pivot_lo)
        {
            // done partitioning; recurse on smaller side first to limit stack
            // depth

            size_t left_size = (size_t)(pivot_lo - lo);
            size_t right_size = (size_t)(hi - pivot_hi);

            if (left_size >= right_size)
            {
                if (right_size > 0)
                {
                    qs_partition(pivot_hi + elem_size, hi, elem_size, compare);
                }
                hi = pivot_lo;
            }
            else
            {
                if (left_size > 0)
                {
                    qs_partition(lo, pivot_lo, elem_size, compare);
                }
                lo = pivot_hi + elem_size;
            }
            goto start;
        }

        // move an element equal to pivot into the pivot area on the left
        pivot_lo -= elem_size;
        rotate_three(right, pivot_lo, left, elem_size);
        right = pivot_hi -= elem_size;
    }
}

/// @brief swap_bytes - swap two elements (elem_size bytes)
/// @param a pointer to first element
/// @param b pointer to second element
static void swap_bytes(unsigned char *a, unsigned char *b, size_t elem_size)
{
    while (elem_size--)
    {
        unsigned char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

/// @brief rotate_three - rotate three elements: a <- c, b <- a, c <- b
/// @param a pointer to first element
/// @param b pointer to second element
/// @param c pointer to third element
static void rotate_three(unsigned char *a, unsigned char *b, unsigned char *c,
                         size_t elem_size)
{
    while (elem_size--)
    {
        unsigned char xa = *a;
        unsigned char xb = *b;
        unsigned char xc = *c;
        *a++ = xc;
        *c++ = xb;
        *b++ = xa;
    }
}
