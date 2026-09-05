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
#include <termios.h>
#include <tomlc17.h>
#include <unistd.h>
#include <vimixutils/minmax.h>
#include <vimixutils/security.h>

#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 64

static bool read_line(char *buffer, size_t size)
{
    if (fgets(buffer, size, stdin) == NULL) return false;

    for (char *p = buffer; *p != '\0'; p++)
    {
        if (*p == '\n' || *p == '\r')
        {
            *p = '\0';
            return true;
        }
    }

    // Reject truncated input and consume the rest so it cannot become the
    // answer to the next prompt.
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
    {
    }
    return false;
}

struct login_config
{
    bool autologin;
    char username[MAX_USERNAME_LEN];
    char autorun_script[PATH_MAX];
};

static void get_login_config(struct login_config *lconfig)
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
        if (autologin_datum.u.str.len > 0 &&
            autologin_datum.u.str.len < sizeof(lconfig->username))
        {
            size_t copy_len = autologin_datum.u.str.len;
            strncpy(lconfig->username, autologin_datum.u.str.ptr, copy_len);
            lconfig->username[copy_len] = '\0';
            lconfig->autologin = true;
        }
        else
        {
            fprintf(stderr, "login: invalid autologin username\n");
        }
    }

    toml_datum_t autorun_script_datum =
        toml_seek(config.toptab, "autologin.autorun_script");
    if (autorun_script_datum.type == TOML_STRING)
    {
        if (autorun_script_datum.u.str.len < sizeof(lconfig->autorun_script))
        {
            size_t copy_len = autorun_script_datum.u.str.len;
            strncpy(lconfig->autorun_script, autorun_script_datum.u.str.ptr,
                    copy_len);
            lconfig->autorun_script[copy_len] = '\0';
        }
        else
        {
            fprintf(stderr, "login: autologin script path too long\n");
        }
    }

    toml_free(config);
}

int main(int argc, char **argv)
{
    struct login_config config = {0};
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
            if (!impersonate_user(config.username, true, true, false)) return 1;
            // start autorun script
            char *shell_argv[] = {"usr/bin/sh", config.autorun_script, NULL};
            execv("/usr/bin/sh", shell_argv);
            fprintf(stderr, "login: execv(%s) failed: %s\n",
                    config.autorun_script, strerror(errno));
        }
        else
        {
            // just login the user
            if (!impersonate_user(config.username, true, true, true)) return 1;
        }
        return 1;
    }

    if (argc < 2)
    {
        printf("username: ");
        fflush(stdout);
        if (!read_line(buf_name, sizeof(buf_name)))
        {
            fprintf(stderr,
                    "login: failed to read username or username too long\n");
            return 1;
        }
        username = buf_name;
    }
    else
    {
        username = argv[1];
    }

    if (username[0] == '\0')
    {
        fprintf(stderr, "login: username must not be empty\n");
        return 1;
    }

    struct termios saved_termios;
    bool restore_echo = tcgetattr(STDIN_FILENO, &saved_termios) == 0;
    if (restore_echo)
    {
        struct termios password_termios = saved_termios;
        password_termios.c_lflag &= ~ECHO;
        restore_echo = tcsetattr(STDIN_FILENO, TCSANOW, &password_termios) == 0;
    }

    printf("password: ");
    fflush(stdout);
    bool password_ok = read_line(buf_password, sizeof(buf_password));
    if (restore_echo)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
        printf("\n");
    }
    if (!password_ok)
    {
        fprintf(stderr,
                "login: failed to read password or password too long\n");
        return 1;
    }
    password = buf_password;

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
