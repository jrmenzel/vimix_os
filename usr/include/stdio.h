/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>
#include <sys/types.h>

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
/// @return
int printf(const char *format, ...);

/// print to file
int fprintf(FILE *stream, const char *format, ...);

/// @brief Gets the file descriptor as an int.
/// @param stream file to get the descriptor from
/// @return file descriptor on success, -1 on failure
int fileno(FILE *stream);

/// @brief flush the stream
int fflush(FILE *stream);

/// @brief opens a file
/// @param filename file name
/// @param modes open/create modes: "r","w","a","r+","w+","a+"
/// @return A file or NULL on failure
FILE *fopen(const char *filename, const char *modes);

/// @brief Closes a file opened with fopen
/// @param stream the file to close
/// @return 0 on success
int fclose(FILE *stream);

/// @brief reads in at most one less than size bytes. Adds a 0-terminator.
/// @param s output string
/// @param size size of s
/// @param stream file to read from
/// @return
char *fgets(char *s, size_t size, FILE *stream);
