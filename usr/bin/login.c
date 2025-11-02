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
#include <vimixutils/security.h>

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

    if (config.autologin == true)
    {
        struct stat st;
        if (stat(config.autorun_script, &st) == 0)
        {
            impersonate_user(config.username, true, true, false);
            // start autorun script
            char *shell_argv[] = {"usr/bin/sh", config.autorun_script, NULL};
            execv("/usr/bin/sh", shell_argv);
            fprintf(stderr, "login: execv(%s) failed: %s\n",
                    config.autorun_script, strerror(errno));
        }
        else
        {
            // just login the user
            impersonate_user(config.username, true, true, true);
        }
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
    password = remove_newline(buf_password);

    // check password
    struct spwd *spw = getspnam(username);
    if (!spw)
    {
        fprintf(stderr, "User '%s' not found in shadow database: %s\n",
                username, strerror(errno));
        return 1;
    }

    if (strcmp(spw->sp_pwdp, password) != 0)
    {
        fprintf(stderr, "login: incorrect password for user '%s'\n", username);
        return 1;
    }

    if (!impersonate_user(username, true, true, true))
    {
        fprintf(stderr, "login: failed to impersonate user '%s'\n", username);
        return 1;
    }
    return 1;  // should not get reached
}
