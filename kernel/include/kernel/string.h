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

/// @brief Copy memory area; areas CAN NOT overlap. Use memmove!
/// @param dst destination address
/// @param src source address
/// @param n bytes to copy
/// @return Pointer to dst
void *memcpy(void *dst, const void *src, size_t n);

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

/// @brief locate character in string
/// @param str String
/// @param c char to search
/// @return pointer to first occurance or NULL if not found.
char *strchr(const char *str, char c);

/// @brief compare two strings
/// @return 0 if the strings are equal
int strcmp(const char *s1, const char *s2);

/// @brief copy a string including the trailing \0
/// @param dst Destinaton pointer.
/// @param src Source string.
char *strcpy(char *dst, const char *src);

/// @brief Calculate the length of a string
/// @param str (Hopefully) NULL-terminated string.
/// @return Length of the string in bytes (EXCLUDING NULL-terminate).
size_t strlen(const char *str);

/// @brief Calculate the length of a string (excluding NUL terminator)
/// @param str (Hopefully) NULL-terminated string.
/// @param maxlen Max chars to check
/// @return Length of the string in bytes (EXCLUDING NULL-terminate), max
/// maxlen.
size_t strnlen(const char *str, size_t maxlen);

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

/// @brief Scan the string for char c, stops after n bytes.
/// @param s The string.
/// @param c Char to scan for.
/// @param n Bytes to scan.
/// @return Pointer to the found location or NULL.
void *memchr(const void *s, int c, size_t n);

/// @brief Scan a string in reverse to find the last occurance of c.
/// @param s NUL terminated string
/// @param c Char to find
/// @return Pointer to the found location or NULL.
char *strrchr(const char *s, int c);

/// @brief Finds first occurance of needle in hackstack. NULL terminator is not
/// compared.
/// @param haystack The string to search in.
/// @param needle The substring to search for.
/// @return Pointer to found substring or NULL if not found.
char *strstr(const char *haystack, const char *needle);

/// @brief Convert a string to an unsigned integer.
/// @param string The string with optional leading whitespaces.
/// @param end If not NULL the position of the first non digit char is returned
/// in end.
/// @param base Must be 10. (should be any base from 2 to 36).
/// @return The int or 0 on error.
unsigned long strtoul(const char *string, char **end, int base);
