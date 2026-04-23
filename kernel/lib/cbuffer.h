/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

/// @brief Circular buffer which overwrites old data when full. Not thread safe.
struct Circular_Buffer
{
    char *buffer;
    size_t head;
    size_t tail;
    size_t capacity;
};

/// @brief Init with externally allocated memory.
/// @param cb Buffer struct to initialize
/// @param buffer Externally allocated memory to use as the buffer, freed
/// externally.
/// @param capacity Size of the buffer in bytes.
void cbuffer_init(struct Circular_Buffer *cb, char *buffer, size_t capacity);

/// @brief Write some bytes into the buffer. If the buffer is full, old data
/// will be overwritten.
/// @param cb Buffer to write to
/// @param data Data to write
/// @param data_len Number of bytes to write
/// @return Number of bytes actually written
size_t cbuffer_write(struct Circular_Buffer *cb, const char *data,
                     size_t data_len);

/// @brief Read some bytes from the buffer. If the buffer is empty, no bytes
/// will be read.
/// @param cb Buffer to read from
/// @param data Buffer to store the read data, allocated by the caller.
/// @param data_len Max number of bytes to read / size of the data buffer.
/// @return Number of bytes actually read
size_t cbuffer_read(struct Circular_Buffer *cb, char *data, size_t data_len);

/// @brief Get the number of bytes available to read from the buffer.
size_t cbuffer_available_data(const struct Circular_Buffer *cb);

/// @brief Get the number of bytes that can be written to the buffer before old
/// data will be overwritten.
size_t cbuffer_available_space(const struct Circular_Buffer *cb);
