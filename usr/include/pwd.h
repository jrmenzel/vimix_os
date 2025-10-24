/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

/// @brief One entry in /etc/passwd
struct passwd
{
    char *pw_name;    ///< User login name.
    char *pw_passwd;  ///< Must be "x".
    uid_t pw_uid;     ///< User ID.
    gid_t pw_gid;     ///< Group ID.
    char *pw_gecos;   ///< Real name.
    char *pw_dir;     ///< Home directory.
    char *pw_shell;   ///< Shell program.
};

/// @param uid User ID to look up.
/// @return Pointer to passwd struct on success, NULL on failure. Sets errno.
struct passwd *getpwuid(uid_t uid);

/// @param name User name to look up.
/// @return Pointer to passwd struct on success, NULL on failure. Sets errno.
struct passwd *getpwnam(const char *name);
