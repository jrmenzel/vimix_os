/* SPDX-License-Identifier: MIT  */
#pragma once

#include <stdint.h>

/// @brief Allocates memory.
/// @param size_in_bytes size of the allocation
/// @return pointer to the memory or NULL on failure
void *malloc(size_t size_in_bytes);

/// @brief Change size of memory pointed to by ptr. Can move the data, will not
/// init new data. Use returned value after use instead of ptr.
/// @param ptr Current allocation. If NULL, realloc will behave like malloc.
/// @param size New size.
/// @return Address of new allocation. NULL on error.
void *realloc(void *ptr, size_t size);

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

#define RAND_MAX 0x7ffffffd
/// @brief Returns a random value between 0 and RAND_MAX
/// @return [0..RAND_MAX]
int rand(void);

/// @brief Sets the random seed value.
/// @param seed New seed.
void srand(unsigned int seed);
