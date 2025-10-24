/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/string.h>

/// @brief Returns a pointer to a string describing the error code (from
/// errno.h)
/// @param errnum Error code (e.g. errno)
/// @return String describing the error. Do not edit.
char *strerror(int errnum);

/// @brief Returns a pointer to a newly allocated copy of the string s.
/// @param s Input string.
/// @return Pointer to the newly allocated string or NULL on error.
char *strdup(const char *s);

/// @brief Extracts tokens from a string.
/// @param str If not NULL, the string to tokenize. If NULL, continues
/// tokenizing the previous string at saveptr.
/// @param delim Delimiter string.
/// @param saveptr Stores the next position to start searching from.
/// @return Pointer to the next token or NULL if no more tokens are found.
char *strtok_r(char *str, const char *delim, char **saveptr);
