/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief Compare memory areas
/// @param s1 Area 1
/// @param s2 Area 2
/// @param n Bytes to compare
/// @return 0 if equal, negative if s1 is smaller than s2, positive otherwise.
int32_t memcmp(const void *s1, const void *s2, size_t n);

/// @brief Copy memory area. Areas may overlap.
/// @param dst destination address
/// @param src source address
/// @param n bytes to copy
/// @return Pointer to dst
void *memmove(void *dst, const void *src, size_t n);

/// @brief Fills memory region with constant value.
/// @param dst Memory to set
/// @param constant Only lower 8bit is used!
/// @param n Number of bytes to set.
/// @return Pointer to dst.
void *memset(void *dst, int32_t constant, size_t n);

/// @brief Like strncpy but guaranteed to NULL-terminate.
/// @param dst Destinaton pointer.
/// @param src Source string.
/// @param n Max chars to copy.
/// @return Pointer to src.
char *safestrcpy(char *dst, const char *str, size_t n);

/// @brief Calculate the length of a string
/// @param str (Hopefully) NULL-terminated string.
/// @return Length of the string in bytes (EXCLUDING NULL-terminate).
size_t strlen(const char *str);

/// @brief Compare strings up to their NULL-terminators or N.
/// @param s1 String s1.
/// @param s2 String s2.
/// @param unsigned_n Max chars to compare.
/// @return 0 if equal, negative if s1 is smaller than s2, positive otherwise.
int strncmp(const char *s1, const char *s2, size_t unsigned_n);

/// @brief Copy a string, but no more than n chars. If the string was shorter,
/// fill remaining space with 0.
/// @param dst Destinaton pointer.
/// @param src Source string.
/// @param n Max chars to copy.
/// @return Pointer to src.
char *strncpy(char *dst, const char *src, size_t n);
