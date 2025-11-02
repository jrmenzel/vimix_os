/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vimixutils/security.h>

bool impersonate_user(const char *user, bool set_groups, bool change_cwd,
                      bool exec_shell)
{
    struct passwd *pw = getpwnam(user);
    if (!pw)
    {
        fprintf(stderr, "User '%s' not found in passwd database: %s\n", user,
                strerror(errno));
        return false;
    }

    if (change_cwd)
    {
        if (chdir(pw->pw_dir) < 0)
        {
            fprintf(stderr, "Failed to change directory to '%s': %s\n",
                    pw->pw_dir, strerror(errno));
            return false;
        }
    }

    if (set_groups)
    {
        if (initgroups(pw->pw_name, pw->pw_gid) < 0)
        {
            fprintf(stderr, "initgroups(%s, %d) failed: %s\n", pw->pw_name,
                    pw->pw_gid, strerror(errno));
            return false;
        }
    }

    if (setuid(pw->pw_uid) < 0)
    {
        fprintf(stderr, "setuid(%d) failed: %s\n", pw->pw_uid, strerror(errno));
        return false;
    }

    if (exec_shell)
    {
        char *binary = strrchr(pw->pw_shell, '/');
        if (binary == NULL)
        {
            fprintf(stderr, "Invalid shell path '%s'\n", pw->pw_shell);
            return false;
        }
        else
        {
            binary++;  // skip '/'
        }
        char *shell_argv[] = {binary, 0};

        // execute the user's shell
        if (execv(pw->pw_shell, shell_argv) < 0)
        {
            fprintf(stderr, "execv(%s) failed: %s\n", pw->pw_shell,
                    strerror(errno));
            return false;
        }
    }

    return true;
}
