/* SPDX-License-Identifier: MIT */
#include <unistd.h>

#include <errno.h>
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
FILE STD_IN = {STDIN_FILENO, _FILE_NO_RETURNED_CHAR};
FILE STD_OUT = {STDOUT_FILENO, _FILE_NO_RETURNED_CHAR};
FILE STD_ERR = {STDERR_FILENO, _FILE_NO_RETURNED_CHAR};
///@}

FILE *stdin = NULL;
FILE *stdout = NULL;
FILE *stderr = NULL;

void init_stdio()
{
    // Setup standard files as defined by the C standard.
    // /usr/bin/init will have these files already opened.
    stdin = &STD_IN;
    stdout = &STD_OUT;
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

static inline bool is_newline(int c) { return (c == '\n' || c == '\r'); }

char *get_from_fd(char *buf, int32_t max, FILE_DESCRIPTOR fd, int returned_char)
{
    int32_t i = 0;

    if (returned_char != _FILE_NO_RETURNED_CHAR)
    {
        buf[i++] = returned_char;
    }

    if (!is_newline(returned_char))
    {
        for (; i + 1 < max;)
        {
            char c;
            int32_t cc = read(fd, &c, 1);
            if (cc <= 0)
            {
                // error or EOF
                break;
            }
            buf[i++] = c;
            if (is_newline(c))
            {
                break;
            }
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

    // Note: "b" is ignored on POSIX systems
    if ((strcmp(modes, "r") == 0) || (strcmp(modes, "rb") == 0))
    {
        flags = O_RDONLY;
    }
    else if ((strcmp(modes, "w") == 0) || (strcmp(modes, "wb") == 0))
    {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        mode |= S_IWUSR;
    }
    else if ((strcmp(modes, "a") == 0) || (strcmp(modes, "ab") == 0))
    {
        flags = O_WRONLY | O_CREAT | O_APPEND;
        mode |= S_IWUSR;
    }
    else if ((strcmp(modes, "r+") == 0) || (strcmp(modes, "rb+") == 0))
    {
        flags = O_RDWR;
    }
    else if ((strcmp(modes, "w+") == 0) || (strcmp(modes, "wb+") == 0))
    {
        flags = O_RDWR | O_CREAT | O_TRUNC;
        mode = mode | S_IRUSR | S_IWUSR;
    }
    else if ((strcmp(modes, "a+") == 0) || (strcmp(modes, "ab+") == 0))
    {
        flags = O_RDWR | O_CREAT | O_APPEND;
        mode = mode | S_IRUSR | S_IWUSR;
    }
    else
    {
        return NULL;
    }

    FILE *file = malloc(sizeof(FILE));
    if (file == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    memset(file, 0, sizeof(FILE));
    file->fd = open(filename, flags, mode);
    file->returned_char = _FILE_NO_RETURNED_CHAR;

    return file;
}

int fclose(FILE *stream)
{
    if (stream == NULL)
    {
        return EOF;
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

    int returned_char = _FILE_NO_RETURNED_CHAR;
    if ((stream->returned_char != _FILE_NO_RETURNED_CHAR) && (size > 0))
    {
        returned_char = stream->returned_char;
        stream->returned_char = _FILE_NO_RETURNED_CHAR;
    }

    return get_from_fd(s, size, stream->fd, returned_char);
}

int fgetc(FILE *stream)
{
    if (stream == NULL)
    {
        return EOF;
    }

    if ((stream->returned_char != _FILE_NO_RETURNED_CHAR))
    {
        int returned_char = stream->returned_char;
        stream->returned_char = _FILE_NO_RETURNED_CHAR;
        return returned_char;
    }

    char c;
    int32_t bytes_read = read(stream->fd, &c, 1);
    if (bytes_read < 1)
    {
        return EOF;
    }
    return c;
}

int ungetc(int c, FILE *stream)
{
    if (stream == NULL)
    {
        return EOF;
    }
    if (stream->returned_char != _FILE_NO_RETURNED_CHAR)
    {
        // only one returned char supported
        return EOF;
    }
    stream->returned_char = c;
    return c;
}

int fseek(FILE *stream, long offset, int whence)
{
    off_t off = lseek(stream->fd, offset, whence);
    return (off < 0) ? -1 : 0;
}

long ftell(FILE *stream)
{
    if (stream == NULL) return -1;

    return lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream) { fseek(stream, 0L, SEEK_SET); }

void perror(const char *s)
{
    if (s != NULL)
    {
        printf("%s; ", s);
    }
    printf("errno: %s (%d)\n", strerror(errno), errno);
}

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
    const size_t _MALLOC_SIZE = 64;

    if (!lineptr || !n || !stream)
    {
        errno = EINVAL;
        return EOF;
    }

    if (*lineptr == NULL)
    {
        *n = 0;
    }

    size_t bytes_read = 0;
    while (true)
    {
        int c = getc(stream);
        if (c == EOF)
        {
            if (bytes_read == 0)
            {
                return EOF;
            }
            break;
        }
        bytes_read++;

        // +1 will reserve one byte extra which is needed to 0-terminate the
        // string
        if ((bytes_read + 1) > *n)
        {
            size_t new_malloc_size = *n + _MALLOC_SIZE;
            *lineptr = realloc(*lineptr, new_malloc_size);
            if (*lineptr == NULL) return ENOMEM;
            *n = new_malloc_size;
        }
        (*lineptr)[bytes_read - 1] = (char)c;
        if (c == delim)
        {
            break;
        }
    }

    (*lineptr)[bytes_read] = 0;
    return bytes_read;
}
