/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

struct group
{
    char *gr_name;    ///< Group name.
    char *gr_passwd;  ///< Password.
    gid_t gr_gid;     ///< Group ID.
    char **gr_mem;    ///< Member list.
};

/// @param gid Group ID to look up.
/// @return Pointer to group struct on success, NULL on failure. Sets errno.
struct group *getgrgid(gid_t gid);

/// @param name Group name to look up.
/// @return Pointer to group struct on success, NULL on failure. Sets errno.
struct group *getgrnam(const char *name);

/// @brief Initialize the group access list by reading the group database
///        and using all groups of which USER is a member. Also include GROUP.
/// @param user User login name, can not be NULL.
/// @param group The users main group ID.
/// @return 0 on success, -1 on failure. Sets errno.
int initgroups(const char *user, gid_t group);

/// @brief Rewind the group-file stream.
void setgrent();

/// @brief Close the group-file stream.
void endgrent();

/// @brief Read an entry from the group-file stream, opening it if necessary.
/// @return Pointer to group struct on success, NULL on failure. Sets errno.
struct group *getgrent();
