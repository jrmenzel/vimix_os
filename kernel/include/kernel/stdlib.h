/* SPDX-License-Identifier: MIT */

#pragma once

#include <kernel/stdint.h>

/// @brief comparator: returns <0, 0 or >0 like strcmp-style comparators
typedef int (*compare_func)(const void *, const void *);

/// @brief Sorts an array using the quicksort algorithm.
/// @param base pointer to the array to sort
/// @param nmemb number of elements
/// @param size size (in bytes) of each element
/// @param compare comparator function compatible with C qsort
void qsort(void *base, size_t n, size_t size, compare_func compare);
