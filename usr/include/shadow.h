/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/types.h>

struct spwd
{
    char *sp_namp;  ///< Login name.
    char *sp_pwdp;  ///< On real UNIX a hashed passphrase, on VIMIX just the
                    ///< password

    // all the following are unused in VIMIX but included for compatibility
    long int sp_lstchg;  ///< Last change date.
    long int sp_min;     ///< Min days between changes.
    long int sp_max;     ///< Max days between changes.
    long int sp_warn;    ///< Num days to warn user to change the password.
    long int sp_inact;   ///< Num days the account may be inactive.
    long int sp_expire;  ///< Num days since 1970-01-01 until account expires.
    unsigned long int sp_flag;  ///< Reserved.
};

/// @brief Get entry from /etc/shadow by user name.
/// @param name User name to look up.
/// @return Pointer to spwd struct on success, NULL on failure. Sets errno.
struct spwd *getspnam(const char *name);
