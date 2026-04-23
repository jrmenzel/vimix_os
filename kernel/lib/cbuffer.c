/* SPDX-License-Identifier: MIT */

#include <lib/cbuffer.h>

void cbuffer_init(struct Circular_Buffer *cb, char *buffer, size_t capacity)
{
    cb->buffer = buffer;
    cb->capacity = capacity;
    cb->head = 0;
    cb->tail = 0;
}

size_t cbuffer_write(struct Circular_Buffer *cb, const char *data,
                     size_t data_len)
{
    size_t bytes_written = 0;
    for (size_t i = 0; i < data_len; ++i)
    {
        size_t next_head = (cb->head + 1) % cb->capacity;
        if (next_head == cb->tail)
        {
            // buffer full: overwrite
            cb->tail = (cb->tail + 1) % cb->capacity;
        }
        cb->buffer[cb->head] = data[i];
        cb->head = next_head;
        bytes_written++;
    }
    return bytes_written;
}

size_t cbuffer_read(struct Circular_Buffer *cb, char *data, size_t data_len)
{
    size_t bytes_read = 0;
    for (size_t i = 0; i < data_len; ++i)
    {
        if (cb->tail == cb->head)
        {
            // buffer empty
            break;
        }
        data[i] = cb->buffer[cb->tail];
        cb->tail = (cb->tail + 1) % cb->capacity;
        bytes_read++;
    }
    return bytes_read;
}

size_t cbuffer_available_data(const struct Circular_Buffer *cb)
{
    if (cb->head >= cb->tail)
    {
        return cb->head - cb->tail;
    }
    else
    {
        return cb->capacity - (cb->tail - cb->head);
    }
}

size_t cbuffer_available_space(const struct Circular_Buffer *cb)
{
    return cb->capacity - cbuffer_available_data(cb) - 1;
}
