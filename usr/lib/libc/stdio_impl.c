/* SPDX-License-Identifier: MIT */
#include <unistd.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "stdio_impl.h"

///@{
/**
 * @brief Standard files input, output and error.
 * They are defined by the C standard (the pointers below).
 * The C standard also defines the FILE_DESCRIPTOR values.
 */
FILE STD_IN;
FILE STD_OUT;
FILE STD_ERR;
///@}

FILE *stdin = NULL;
FILE *stdout = NULL;
FILE *stderr = NULL;

void init_stdio()
{
    // Setup standard files as defined by the C standard.
    // /init will have these files already opened.
    STD_IN.fd = STDIN_FILENO;
    stdin = &STD_IN;
    STD_OUT.fd = STDOUT_FILENO;
    stdout = &STD_OUT;
    STD_ERR.fd = STDERR_FILENO;
    stderr = &STD_ERR;
}

int fileno(FILE *stream)
{
    if (stream == NULL)
    {
        return -1;
    }
    return (int)stream->fd;
}

char *get_from_fd(char *buf, int32_t max, FILE_DESCRIPTOR fd)
{
    int32_t i, cc;
    char c;

    for (i = 0; i + 1 < max;)
    {
        cc = read(fd, &c, 1);
        if (cc < 1)
        {
            break;
        }
        buf[i++] = c;
        if (c == '\n' || c == '\r')
        {
            break;
        }
    }
    buf[i] = '\0';
    return buf;
}

int fflush(FILE *stream) { return 0; }

FILE *fopen(const char *filename, const char *modes)
{
    int32_t flags = 0;
    mode_t mode = S_IFREG;

    if (strcmp(modes, "r") == 0)
    {
        flags = O_RDONLY;
    }
    else if (strcmp(modes, "w") == 0)
    {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        mode |= S_IWUSR;
    }
    else if (strcmp(modes, "a") == 0)
    {
        flags = O_WRONLY | O_CREAT | O_APPEND;
        mode |= S_IWUSR;
    }
    else if (strcmp(modes, "r+") == 0)
    {
        flags = O_RDWR;
    }
    else if (strcmp(modes, "w+") == 0)
    {
        flags = O_RDWR | O_CREAT | O_TRUNC;
        mode = mode | S_IRUSR | S_IWUSR;
    }
    else if (strcmp(modes, "a+") == 0)
    {
        flags = O_RDWR | O_CREAT | O_APPEND;
        mode = mode | S_IRUSR | S_IWUSR;
    }
    else
    {
        return NULL;
    }

    FILE *file = malloc(sizeof(FILE));
    file->fd = open(filename, flags, mode);

    return file;
}

int fclose(FILE *stream)
{
    if (stream == NULL)
    {
        return -1;
    }

    int return_value = close(stream->fd);
    free(stream);

    return return_value;
}

char *fgets(char *s, size_t size, FILE *stream)
{
    if (stream == NULL)
    {
        return NULL;
    }

    return get_from_fd(s, size, stream->fd);
}
