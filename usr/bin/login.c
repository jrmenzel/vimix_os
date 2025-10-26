/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <shadow.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tomlc17.h>
#include <unistd.h>
#include <vimixutils/minmax.h>

#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 64

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

struct login_config
{
    bool autologin;
    char username[MAX_USERNAME_LEN];
    char autorun_script[PATH_MAX];
};

void get_login_config(struct login_config *lconfig)
{
    toml_result_t config = toml_parse_file_ex("/etc/login.conf");
    if (!config.ok)
    {
        fprintf(stderr, "login: failed to parse /etc/login.conf: %s\n",
                config.errmsg);
        return;
    }

    toml_datum_t autologin_datum =
        toml_seek(config.toptab, "autologin.username");
    if (autologin_datum.type == TOML_STRING)
    {
        size_t copy_len =
            min(autologin_datum.u.str.len, sizeof(lconfig->username) - 1);
        strncpy(lconfig->username, autologin_datum.u.str.ptr, copy_len);

        lconfig->username[copy_len] = '\0';
        lconfig->autologin = true;
    }

    toml_datum_t autorun_script_datum =
        toml_seek(config.toptab, "autologin.autorun_script");
    if (autorun_script_datum.type == TOML_STRING)
    {
        size_t copy_len = min(autorun_script_datum.u.str.len,
                              sizeof(lconfig->autorun_script) - 1);
        strncpy(lconfig->autorun_script, autorun_script_datum.u.str.ptr,
                copy_len);

        lconfig->autorun_script[copy_len] = '\0';
    }

    toml_free(config);
}

int main(int argc, char **argv)
{
    struct login_config config;
    get_login_config(&config);

    char *username = NULL;
    char *password = NULL;
    char buf_name[MAX_USERNAME_LEN];
    char buf_password[MAX_PASSWORD_LEN];

    uid_t uid = getuid();
    if (uid != 0)
    {
        fprintf(stderr, "login: must be run as root\n");
        return 1;
    }

    if (config.autologin == false)
    {
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
        password = remove_newline(buf_password);
    }
    else
    {
        username = config.username;
        password = "";  // no password needed for autologin
    }

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

    if ((config.autologin == false) && (strcmp(spw->sp_pwdp, password) != 0))
    {
        fprintf(stderr, "login: incorrect password for user '%s'\n", username);
        return 1;
    }

    chdir(pw->pw_dir);

    if (initgroups(pw->pw_name, pw->pw_gid) < 0)
    {
        fprintf(stderr, "login: initgroups(%s, %d) failed: %s\n", pw->pw_name,
                pw->pw_gid, strerror(errno));
        return 1;
    }

    if (setuid(pw->pw_uid) < 0)
    {
        fprintf(stderr, "login: setuid(%d) failed: %s\n", pw->pw_uid,
                strerror(errno));
        return 1;
    }

    char *binary = strrchr(pw->pw_shell, '/');
    if (binary == NULL)
    {
        fprintf(stderr, "login: invalid shell path '%s'\n", pw->pw_shell);
        return 1;
    }
    else
    {
        binary++;  // skip '/'
    }
    char *shell_argv[] = {binary, 0, 0};

    // if autorun script is specified, and it exists, set it as argv[1]
    if (config.autologin && (strlen(config.autorun_script) > 0))
    {
        struct stat st;
        if (stat(config.autorun_script, &st) == 0)
        {
            shell_argv[1] = config.autorun_script;
        }
    }

    // execute the user's shell
    if (execv(pw->pw_shell, shell_argv) < 0)
    {
        fprintf(stderr, "login: execv(%s) failed: %s\n", pw->pw_shell,
                strerror(errno));
        return 1;
    }
    return 1;  // should not be reached
}
