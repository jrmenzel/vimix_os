/* SPDX-License-Identifier: MIT */
#pragma once

#include <fs/dentry.h>
#include <kernel/kernel.h>

enum Lookup_Mode
{
    /// @brief If the final element of a path is a symlink,
    /// follow it and report back the file it points to.
    FOLLOW_FINAL_SYMLINK,

    /// @brief If the final element of the path is a symlink,
    /// report back the symlink itself.
    DONT_FOLLOW_FINAL_SYMLINK
};

/// @brief get dentry based on the path.
/// Shortly locks every dentry on the path, so don't hold any dentry locks when
/// calling to avoid dead-locks!
/// @param path Absolute or CWD relative path.
/// @param mode Lookup mode.
/// @param error On error, set to negative error code.
/// @return NULL on failure. Returned dentry has an increased ref
/// count (release with dentry_put()). (NOT locked)
struct dentry *dentry_from_path_mode(const char *path,
                                     enum Lookup_Mode lookup_mode,
                                     syserr_t *error);

/// @brief Resolve path relative to dp. dp must carry a reference, which this
/// function consumes on both success and failure.
/// All intermediate symlinks are followed.
/// lookup_mode controls only the final component.
struct dentry *dentry_from_path_at(struct dentry *dp, const char *path,
                                   enum Lookup_Mode lookup_mode,
                                   syserr_t *error);
