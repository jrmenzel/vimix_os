/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Functions return a pointer to this global struct which makes them not
// thread-safe.
#define MAX_LINE_LEN 128
char g_pw_line[MAX_LINE_LEN];  // g_passwd line buffer points into this
struct passwd g_passwd;

struct passwd *getpw_by_name_or_id(const char *name, uid_t uid)
{
    FILE *f = fopen("/etc/passwd", "r");
    if (!f)
    {
        return NULL;  // errno is still set by fopen
    }

    while (fgets(g_pw_line, MAX_LINE_LEN, f) != NULL)
    {
        // parse line:
        // format: name:passwd:uid:gid:gecos:dir:shell
        char *saveptr = NULL;
        char *pw_name = strtok_r(g_pw_line, ":\n", &saveptr);
        char *pw_passwd = strtok_r(NULL, ":\n", &saveptr);
        char *pw_uid_str = strtok_r(NULL, ":\n", &saveptr);
        char *pw_gid_str = strtok_r(NULL, ":\n", &saveptr);
        char *pw_gecos = strtok_r(NULL, ":\n", &saveptr);
        char *pw_dir = strtok_r(NULL, ":\n", &saveptr);
        char *pw_shell = strtok_r(NULL, ":\n", &saveptr);

        if (!pw_name || !pw_passwd || !pw_uid_str || !pw_gid_str || !pw_gecos ||
            !pw_dir || !pw_shell)
        {
            continue;  // malformed line
        }

        uid_t pw_uid = (uid_t)atoi(pw_uid_str);
        if ((pw_uid == uid) || (name && (strcmp(pw_name, name) == 0)))
        {
            // found user
            g_passwd.pw_name = pw_name;
            g_passwd.pw_passwd = pw_passwd;
            g_passwd.pw_uid = pw_uid;
            g_passwd.pw_gid = (gid_t)atoi(pw_gid_str);
            g_passwd.pw_gecos = pw_gecos;
            g_passwd.pw_dir = pw_dir;
            g_passwd.pw_shell = pw_shell;

            fclose(f);
            return &g_passwd;
        }
    }

    fclose(f);
    errno = ENOENT;
    return NULL;
}

struct passwd *getpwuid(uid_t uid) { return getpw_by_name_or_id(NULL, uid); }

struct passwd *getpwnam(const char *name)
{
    if (!name)
    {
        errno = EINVAL;
        return NULL;
    }

    return getpw_by_name_or_id(name, -1);
}
