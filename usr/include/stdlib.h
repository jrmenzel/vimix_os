/* SPDX-License-Identifier: MIT  */
#pragma once

#include <stdint.h>

/// @brief Allocates memory.
/// @param size_in_bytes size of the allocation
/// @return pointer to the memory or NULL on failure
void *malloc(size_t size_in_bytes);

/// @brief Free memory allocated by malloc
/// @param ptr pointer returned by malloc
void free(void *ptr);

// C99 7.20.6.1
// Returns absolute value of j. Unless it's the lowest negative value
// as that does not have a positive integer representation. Behavior is
// undefined in that case.
int abs(int j);
long int labs(long int j);
long long int llabs(long long int j);

/// convert a string to an integer
int32_t atoi(const char *string);

/// @brief Registers a function to be called at process termination. If multiple
/// functions are registered, they are called in reverse order.
/// @param function The function to be called.
/// @return 0 on sucess, -1 if too many functions are registered.
int atexit(void (*function)(void));
