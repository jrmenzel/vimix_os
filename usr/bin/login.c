/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *remove_newline(char *s)
{
    char *p = s;
    while (*p != '\0')
    {
        if (*p == '\n' || *p == '\r')
        {
            *p = '\0';
            break;
        }
        p++;
    }
    return s;
}

int main(int argc, char **argv)
{
    char *username = NULL;
    char buf_name[64];
    char buf_password[64];

    uid_t uid = getuid();
    if (uid != 0)
    {
        fprintf(stderr, "login: must be run as root\n");
        return 1;
    }

    if (argc < 2)
    {
        printf("username: ");
        if (fgets(buf_name, sizeof(buf_name), stdin) == NULL)
        {
            fprintf(stderr, "login: failed to read username\n");
            return 1;
        }
        username = remove_newline(buf_name);
    }
    else
    {
        username = argv[1];
    }

    printf("password: ");
    if (fgets(buf_password, sizeof(buf_password), stdin) == NULL)
    {
        fprintf(stderr, "login: failed to read password\n");
        return 1;
    }
    char *password = remove_newline(buf_password);

    struct spwd *spw = getspnam(username);
    if (!spw)
    {
        fprintf(stderr, "User '%s' not found in shadow database: %s\n",
                username, strerror(errno));
        return 1;
    }
    struct passwd *pw = getpwnam(spw->sp_namp);
    if (!pw)
    {
        fprintf(stderr, "User '%s' not found in passwd database: %s\n",
                username, strerror(errno));
        return 1;
    }

    if (strcmp(spw->sp_pwdp, password) != 0)
    {
        fprintf(stderr, "login: incorrect password for user '%s'\n", username);
        return 1;
    }

    chdir(pw->pw_dir);

    if (initgroups(pw->pw_name, pw->pw_gid) < 0)
    {
        fprintf(stderr, "su: initgroups(%s, %d) failed: %s\n", pw->pw_name,
                pw->pw_gid, strerror(errno));
        return 1;
    }

    if (setuid(pw->pw_uid) < 0)
    {
        fprintf(stderr, "su: setuid(%d) failed: %s\n", pw->pw_uid,
                strerror(errno));
        return 1;
    }

    char *binary = strrchr(pw->pw_shell, '/');
    if (binary == NULL)
    {
        fprintf(stderr, "su: invalid shell path '%s'\n", pw->pw_shell);
        return 1;
    }
    else
    {
        binary++;  // skip '/'
    }
    char *shell_argv[] = {binary, 0};
    if (execv(pw->pw_shell, shell_argv) < 0)
    {
        fprintf(stderr, "su: execv(%s) failed: %s\n", pw->pw_shell,
                strerror(errno));
        return 1;
    }
    return 1;  // should not be reached
}
