/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#define EOF (-1)

struct _FILE
{
    FILE_DESCRIPTOR fd;
};

typedef struct _FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/// @brief print formatted
/// @param format
/// @return Chars written
int printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

/// @brief Print to file
/// @param stream The file to write to.
/// @param format printf-like format
/// @return Chars written.
int fprintf(FILE *stream, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

/// @brief Gets the file descriptor as an int.
/// @param stream file to get the descriptor from
/// @return file descriptor on success, -1 on failure
int fileno(FILE *stream);

/// @brief flush the stream
int fflush(FILE *stream);

/// @brief Opens a file
/// @param filename file name
/// @param modes open/create modes: "r","w","a","r+","w+","a+"
///              'b' is supported but ignored like on POSIX systems.
/// @return A file or NULL on failure
FILE *fopen(const char *filename, const char *modes);

/// @brief Closes a file opened with fopen
/// @param stream the file to close
/// @return 0 on success
int fclose(FILE *stream);

/// @brief Reads in a string, at most size-1 bytes. Adds a 0-terminator.
/// @param s output string
/// @param size size of s
/// @param stream file to read from
/// @return pointer to the string s
char *fgets(char *s, size_t size, FILE *stream);

/// @brief Set the file position indicator of stream to whence +/- offset.
/// @param stream File stream to set seek position from.
/// @param offset Offset in bytes.
/// @param whence SEEK_SET (file beginning), SEEK_CUR (current seek position),
/// or SEEK_END (file end)
/// @return 0 on success
int fseek(FILE *stream, long offset, int whence);

/// @brief Returns current file position indicator.
/// @param stream
/// @return seek position or -1 on error
long ftell(FILE *stream);

/// @brief Set file position indicator to beginning of the file.
/// @param stream Stream to rewind.
void rewind(FILE *stream);
