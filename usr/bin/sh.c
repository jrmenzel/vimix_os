/* SPDX-License-Identifier: MIT */

// Shell.
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(BUILD_ON_HOST)
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAX_EXEC_ARGSS 10

struct cmd
{
    int type;
};

struct execcmd
{
    int type;
    char *argv[MAX_EXEC_ARGSS];
    char *eargv[MAX_EXEC_ARGSS];
};

struct redircmd
{
    int type;
    struct cmd *cmd;
    char *file;
    char *efile;
    int mode;
    int fd;
};

struct pipecmd
{
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct listcmd
{
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct backcmd
{
    int type;
    struct cmd *cmd;
};

int fork1();  // Fork but panics on failure.
void sh_panic(char *);
struct cmd *parsecmd(char *);
void runcmd(struct cmd *) __attribute__((noreturn));

int build_full_path(char *dst, const char *path, const char *file)
{
    strncpy(dst, path, PATH_MAX);
    size_t len = strlen(dst);
    if (dst[len - 1] != '/')
    {
        dst[len] = '/';
        len++;
    }
    size_t name_len = strlen(file);
    if (len + name_len > PATH_MAX - 1)
    {
        return -1;
    }
    strncpy(dst + len, file, PATH_MAX - len);

    return 0;
}

const char *search_path[] = {"/usr/bin", "/usr/local/bin", NULL};

void execute_command(struct execcmd *ecmd)
{
    if (ecmd->argv[0] == 0) exit(1);

    if (ecmd->argv[0][0] == '.' || ecmd->argv[0][0] == '/')
    {
        // don't use the search path, e.g. for "./foo" or "/usr/bin/bar"
        execv(ecmd->argv[0], ecmd->argv);
    }
    else
    {
        char full_path[PATH_MAX];
        for (size_t search_path_index = 0;
             search_path[search_path_index] != NULL; search_path_index++)
        {
            const char *current_search_path = search_path[search_path_index];
            build_full_path(full_path, current_search_path, ecmd->argv[0]);
            execv(full_path, ecmd->argv);
        }
        // execv only returns on error
        fprintf(stderr, "exec %s failed (%s)\n", ecmd->argv[0],
                strerror(errno));
    }
}

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd)
{
    int p[2];
    struct backcmd *bcmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0) exit(1);

    switch (cmd->type)
    {
        default: sh_panic("runcmd");

        case EXEC: execute_command((struct execcmd *)cmd); break;

        case REDIR:
            rcmd = (struct redircmd *)cmd;
            // If one of the standard files (0,1,2) are closed,
            // the next opend file is guaranteed to get its fd (as it's
            // simply the smallest unused).
            // This will not work on e.g. Linux, use dup2() there.
            close(rcmd->fd);
            if (open(rcmd->file, rcmd->mode, 0755) < 0)
            {
                fprintf(stderr, "open %s failed\n", rcmd->file);
                exit(1);
            }
            runcmd(rcmd->cmd);
            break;

        case LIST:
            lcmd = (struct listcmd *)cmd;
            if (fork1() == 0) runcmd(lcmd->left);
            wait(NULL);
            runcmd(lcmd->right);
            break;

        case PIPE:
            pcmd = (struct pipecmd *)cmd;
            if (pipe(p) < 0) sh_panic("pipe");
            if (fork1() == 0)
            {
                close(1);
                int fd = dup(p[1]);
                if (fd != 1) exit(1);

                close(p[0]);
                close(p[1]);
                runcmd(pcmd->left);
            }
            if (fork1() == 0)
            {
                close(0);
                int fd = dup(p[0]);
                if (fd != 0) exit(1);

                close(p[0]);
                close(p[1]);
                runcmd(pcmd->right);
            }
            close(p[0]);
            close(p[1]);
            wait(NULL);
            wait(NULL);
            break;

        case BACK:
            bcmd = (struct backcmd *)cmd;
            if (fork1() == 0) runcmd(bcmd->cmd);
            break;
    }
    exit(0);
}

int getcmd(char *buf, int nbuf, bool print_prompt)
{
    if (print_prompt)
    {
        ssize_t w = write(STDOUT_FILENO, "$ ", 2);
        if (w != 2) return -1;
    }
    memset(buf, 0, nbuf);
    char *s = fgets(buf, nbuf, stdin);
    if (s == NULL || buf[0] == 0)  // error or EOF
    {
        return -1;
    }
    return 0;
}

int is_blank_string(const char *s)
{
    char c = 0;
    while ((c = *s++))
    {
        if (!isspace(c))
        {
            return 0;
        }
    }
    return 1;
}

int main(int argc, const char *argv[])
{
    static char buf[100];
    bool print_prompt = true;

    if (argc == 2)
    {
        close(STDIN_FILENO);
        int fd = open(argv[1], O_RDONLY);
        if (fd != STDIN_FILENO)
        {
            fprintf(stderr, "Error reading %s\n", argv[1]);
            return 1;
        }
        print_prompt = false;
    }
    else if (argc > 2)
    {
        fprintf(stderr, "Error: usage: sh [script]\n");
        return 1;
    }

    // Read and run input commands.
    while (getcmd(buf, sizeof(buf), print_prompt) >= 0)
    {
        // check for comments:
        for (size_t i = 0; i < sizeof(buf); ++i)
        {
            if (buf[i] == 0) break;
            if (buf[i] == '#')
            {
                buf[i] = 0;  // terminate string where the comment starts
                break;
            }
        }
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
        {
            // Chdir must be called by the parent, not the child.
            buf[strlen(buf) - 1] = 0;  // chop \n
            if (chdir(buf + 3) < 0) fprintf(stderr, "cannot cd %s\n", buf + 3);
            continue;
        }
        if (is_blank_string(buf))
        {
            // ignore blank lines and don't fork just to return
            continue;
        }
        if (fork1() == 0)
        {
            runcmd(parsecmd(buf));
        }

        int status = 0;
        wait(&status);
        status = WEXITSTATUS(status);
        // printf("return code: %d\n", status);
    }
    return 0;
}

void sh_panic(char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(1);
}

int fork1()
{
    pid_t pid = fork();
    if (pid == -1) sh_panic("fork");
    return pid;
}

struct cmd *execcmd()
{
    struct execcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = EXEC;
    return (struct cmd *)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode,
                     int fd)
{
    struct redircmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd = fd;
    return (struct cmd *)cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right)
{
    struct pipecmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

struct cmd *listcmd(struct cmd *left, struct cmd *right)
{
    struct listcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

struct cmd *backcmd(struct cmd *subcmd)
{
    struct backcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    return (struct cmd *)cmd;
}

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
    char *s;
    int ret;

    s = *ps;
    while (s < es && strchr(whitespace, *s)) s++;
    if (q) *q = s;
    ret = *s;
    switch (*s)
    {
        case 0: break;
        case '|':
        case '(':
        case ')':
        case ';':
        case '&':
        case '<': s++; break;
        case '>':
            s++;
            if (*s == '>')
            {
                ret = '+';
                s++;
            }
            break;
        default:
            ret = 'a';
            while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
                s++;
            break;
    }
    if (eq) *eq = s;

    while (s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return ret;
}

int peek(char **ps, char *es, char *toks)
{
    char *s;

    s = *ps;
    while (s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *parsecmd(char *s)
{
    char *es;
    struct cmd *cmd;

    es = s + strlen(s);
    cmd = parseline(&s, es);
    peek(&s, es, "");
    if (s != es)
    {
        fprintf(stderr, "leftovers: %s\n", s);
        sh_panic("syntax");
    }
    nulterminate(cmd);
    return cmd;
}

struct cmd *parseline(char **ps, char *es)
{
    struct cmd *cmd;

    cmd = parsepipe(ps, es);
    while (peek(ps, es, "&"))
    {
        gettoken(ps, es, 0, 0);
        cmd = backcmd(cmd);
    }
    if (peek(ps, es, ";"))
    {
        gettoken(ps, es, 0, 0);
        cmd = listcmd(cmd, parseline(ps, es));
    }
    return cmd;
}

struct cmd *parsepipe(char **ps, char *es)
{
    struct cmd *cmd;

    cmd = parseexec(ps, es);
    if (peek(ps, es, "|"))
    {
        gettoken(ps, es, 0, 0);
        cmd = pipecmd(cmd, parsepipe(ps, es));
    }
    return cmd;
}

struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es)
{
    int tok;
    char *q, *eq;

    while (peek(ps, es, "<>"))
    {
        tok = gettoken(ps, es, 0, 0);
        if (gettoken(ps, es, &q, &eq) != 'a')
            sh_panic("missing file for redirection");
        switch (tok)
        {
            case '<': cmd = redircmd(cmd, q, eq, O_RDONLY, 0); break;
            case '>':
                cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREAT | O_TRUNC, 1);
                break;
            case '+':  // >>
                cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREAT, 1);
                break;
        }
    }
    return cmd;
}

struct cmd *parseblock(char **ps, char *es)
{
    struct cmd *cmd;

    if (!peek(ps, es, "(")) sh_panic("parseblock");
    gettoken(ps, es, 0, 0);
    cmd = parseline(ps, es);
    if (!peek(ps, es, ")")) sh_panic("syntax - missing )");
    gettoken(ps, es, 0, 0);
    cmd = parseredirs(cmd, ps, es);
    return cmd;
}

struct cmd *parseexec(char **ps, char *es)
{
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    if (peek(ps, es, "(")) return parseblock(ps, es);

    ret = execcmd();
    cmd = (struct execcmd *)ret;

    argc = 0;
    ret = parseredirs(ret, ps, es);
    while (!peek(ps, es, "|)&;"))
    {
        if ((tok = gettoken(ps, es, &q, &eq)) == 0) break;
        if (tok != 'a') sh_panic("syntax");
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        if (argc >= MAX_EXEC_ARGSS) sh_panic("too many args");
        ret = parseredirs(ret, ps, es);
    }
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;
    return ret;
}

/// NUL-terminate all the counted strings.
struct cmd *nulterminate(struct cmd *cmd)
{
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0) return 0;

    switch (cmd->type)
    {
        case EXEC:
            ecmd = (struct execcmd *)cmd;
            for (i = 0; ecmd->argv[i]; i++) *ecmd->eargv[i] = 0;
            break;

        case REDIR:
            rcmd = (struct redircmd *)cmd;
            nulterminate(rcmd->cmd);
            *rcmd->efile = 0;
            break;

        case PIPE:
            pcmd = (struct pipecmd *)cmd;
            nulterminate(pcmd->left);
            nulterminate(pcmd->right);
            break;

        case LIST:
            lcmd = (struct listcmd *)cmd;
            nulterminate(lcmd->left);
            nulterminate(lcmd->right);
            break;

        case BACK:
            bcmd = (struct backcmd *)cmd;
            nulterminate(bcmd->cmd);
            break;
    }
    return cmd;
}
