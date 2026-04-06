/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/dentry.h>
#include <kernel/kernel.h>

/// @brief get dentry based on the path.
/// Shortly locks every dentry on the path, so don't hold any dentry locks when
/// calling to avoid dead-locks!
/// @param path Absolute or CWD relative path.
/// @param error On error, set to negative error code.
/// @return NULL on failure. Returned dentry has an increased ref
/// count (release with dentry_put()). (NOT locked)
struct dentry *dentry_from_path(const char *path, syserr_t *error);
