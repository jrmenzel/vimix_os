/* SPDX-License-Identifier: MIT */

#if defined(BUILD_ON_HOST)
// get_current_dir_name() requires _GNU_SOURCE
#define _GNU_SOURCE
#include <linux/limits.h>
#else
#include <kernel/limits.h>
#endif

// Shell.
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <kernel/container_of.h>
#include <kernel/list.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vimixutils/minmax.h>
#include <vimixutils/path.h>

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAX_EXEC_ARGSS 16
#define INPUT_BUF_SIZE 100

struct Shell_History_Entry
{
    struct list_head head;
    size_t line_length;
    char line[];
};

#define hist_entry_from_list(ptr) \
    container_of(ptr, struct Shell_History_Entry, head)

struct Shell_History
{
    struct list_head list;
    struct list_head *selected;
    size_t entry_count;
    char *file_name_to_save_to;
};

struct Shell_History g_shell_history;

void shell_history_load();
static bool write_buffer(int fd, const char *buf, size_t count);

void shell_history_init()
{
    list_init(&g_shell_history.list);
    // The list head represents the line currently being edited, just after
    // the newest history entry.
    g_shell_history.selected = &g_shell_history.list;
    g_shell_history.entry_count = 0;
    g_shell_history.file_name_to_save_to = NULL;

    shell_history_load();
}

#define SHELL_HISTORY_MAX_ENTRIES 64

void shell_history_add(const char *buf, size_t buf_len)
{
    // skip empty line:
    if (buf_len == 0) return;

    // skip duplicate:
    if (!list_empty(&g_shell_history.list))
    {
        struct Shell_History_Entry *latest_entry =
            hist_entry_from_list(g_shell_history.list.prev);

        if ((latest_entry->line_length == buf_len) &&
            (memcmp(latest_entry->line, buf, buf_len) == 0))
        {
            g_shell_history.selected = &g_shell_history.list;
            return;
        }
    }

    // add new:
    struct Shell_History_Entry *new_entry =
        malloc(sizeof(struct Shell_History_Entry) + buf_len + 1);
    if (new_entry == NULL)
    {
        fprintf(stderr, "Shell: out of memory\n");
        return;
    }

    list_init(&new_entry->head);
    memcpy(new_entry->line, buf, buf_len + 1);
    new_entry->line[buf_len] = 0;
    new_entry->line_length = buf_len;

    list_add_tail(&new_entry->head, &g_shell_history.list);

    g_shell_history.selected = &g_shell_history.list;

    // clear old entries:
    g_shell_history.entry_count++;
    if (g_shell_history.entry_count > SHELL_HISTORY_MAX_ENTRIES)
    {
        struct Shell_History_Entry *entry =
            hist_entry_from_list(g_shell_history.list.next);
        list_del(&entry->head);
        free(entry);
        g_shell_history.entry_count--;
    }
}

void shell_history_select_previous()
{
    struct list_head *pos = g_shell_history.selected->prev;
    if (pos == &g_shell_history.list)
    {
        // oldest entry hit, no change in selection
        return;
    }

    g_shell_history.selected = pos;
}

void shell_history_select_next()
{
    if (g_shell_history.selected == &g_shell_history.list)
    {
        return;
    }

    struct list_head *pos = g_shell_history.selected->next;
    if (pos == &g_shell_history.list)
    {
        // Move past the newest entry to the line currently being edited.
        g_shell_history.selected = &g_shell_history.list;
        return;
    }

    g_shell_history.selected = pos;
}

enum Shell_History_Selection
{
    NO_ENTRY,
    NEWEST_ENTRY,
    HISTORY_ENTRY
};

enum Shell_History_Selection shell_history_peek_selection_type()
{
    if (g_shell_history.selected == &g_shell_history.list)
    {
        return NO_ENTRY;
    }

    if (g_shell_history.selected == g_shell_history.list.prev)
    {
        return NEWEST_ENTRY;
    }

    return HISTORY_ENTRY;
}

size_t shell_history_get_selected(char *dst, size_t dst_len)
{
    if ((g_shell_history.selected == &g_shell_history.list) || (dst_len == 0))
    {
        return 0;
    }

    struct Shell_History_Entry *entry =
        hist_entry_from_list(g_shell_history.selected);
    size_t copy_amount = min(dst_len - 1, entry->line_length);
    memcpy(dst, entry->line, copy_amount);
    dst[copy_amount] = 0;
    return copy_amount;
}

// loads the history, quietly ignores errors
void shell_history_load()
{
    // for now the CWD, assuming the shell was started in the users home
    char *cwd = get_current_dir_name();
    if (cwd == NULL) return;

    // add "/.config/sh_history" to the cwd path
    static const char history_file_suffix[] = "/.config/sh_history";
    size_t file_name_length = strlen(cwd) + sizeof(history_file_suffix);
    char *file_name = malloc(file_name_length);
    if (file_name == NULL)
    {
        free(cwd);
        return;
    }
    snprintf(file_name, file_name_length, "%s%s", cwd, history_file_suffix);
    free(cwd);

    // if file does not exist, set file_name_to_save_to to NULL
    FILE *history_file = fopen(file_name, "r");
    if (history_file == NULL)
    {
        free(file_name);
        g_shell_history.file_name_to_save_to = NULL;
        return;
    }
    g_shell_history.file_name_to_save_to = file_name;

    // if file exists, read lines and call shell_history_add()
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    while ((line_length = getline(&line, &line_capacity, history_file)) >= 0)
    {
        if (line == NULL) break;

        // trailing new line chars to 0-terminator
        while ((line_length > 0) && ((line[line_length - 1] == '\n') ||
                                     (line[line_length - 1] == '\r')))
        {
            line[--line_length] = 0;
        }
        shell_history_add(line, (size_t)line_length);
    }
    free(line);
    fclose(history_file);
}

void shell_history_save()
{
    if (g_shell_history.file_name_to_save_to == NULL) return;

    // overwrite existing file, store history
    int fd = open(g_shell_history.file_name_to_save_to, O_WRONLY | O_TRUNC);
    if (fd < 0) return;

    struct list_head *pos;
    list_for_each(pos, &g_shell_history.list)
    {
        struct Shell_History_Entry *entry = hist_entry_from_list(pos);
        if (!write_buffer(fd, entry->line, entry->line_length) ||
            !write_buffer(fd, "\n", 1))
        {
            break;
        }
    }
    close(fd);
}

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

void execute_command(struct execcmd *ecmd)
{
    if (ecmd->argv[0] == NULL) exit(1);

    const char *full_path = find_program_in_path(ecmd->argv[0]);
    if (full_path == NULL)
    {
        fprintf(stderr, "exec %s failed (%s)\n", ecmd->argv[0],
                strerror(errno));
        return;
    }
    execv(full_path, ecmd->argv);

    fprintf(stderr, "exec %s failed (%s)\n", ecmd->argv[0], strerror(errno));
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
            if (open(rcmd->file, rcmd->mode, 0644) < 0)
            {
                fprintf(stderr, "open %s failed, %s\n", rcmd->file,
                        strerror(errno));
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
                close(STDOUT_FILENO);
                int fd = dup(p[1]);
                if (fd != 1) exit(1);

                close(p[0]);
                close(p[1]);
                runcmd(pcmd->left);
            }
            if (fork1() == 0)
            {
                close(STDIN_FILENO);
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
    exit(EXIT_SUCCESS);
}

// faster way to print a string than printf with its parsing
// and does not have to be 0-terminated
static bool write_buffer(int fd, const char *buf, size_t count)
{
    while (count > 0)
    {
        ssize_t written = write(fd, buf, count);
        if (written <= 0) return false;
        buf += written;
        count -= written;
    }
    return true;
}

static void set_cursor(size_t cursor_pos)
{
    char sequence[32];
    int sequence_length =
        snprintf(sequence, sizeof(sequence), "\033[%zuD", cursor_pos);
    if (sequence_length > 0)
    {
        write_buffer(STDOUT_FILENO, sequence, (size_t)sequence_length);
    }
}

static void redraw_input(const char *prompt, const char *buf, size_t length,
                         size_t cursor_pos)
{
    static const char clear_line[] = "\r\033[2K";
    write_buffer(STDOUT_FILENO, clear_line, sizeof(clear_line) - 1);
    write_buffer(STDOUT_FILENO, prompt, strlen(prompt));
    write_buffer(STDOUT_FILENO, buf, length);

    if (cursor_pos < length)
    {
        set_cursor(length - cursor_pos);
    }
}

enum Shell_Escape_State
{
    ESCAPE_STATE_NONE,
    ESCAPE_STATE_ESCAPE,
    ESCAPE_STATE_CSI,
    ESCAPE_STATE_CSI_PARAMETER,
    ESCAPE_STATE_SS3,
};

// Read one command in noncanonical mode, this means the kernels console sends
// individual bytes, not a full input line.
// This is done to react to e.g. the arrow up key to recall a previous command.
static int read_interactive_line(char *buf, size_t capacity, const char *prompt)
{
    struct termios saved_termios;
    if ((capacity < 2) || (tcgetattr(STDIN_FILENO, &saved_termios) < 0))
    {
        return -1;
    }

    struct termios edit_termios = saved_termios;
    edit_termios.c_lflag &= ~(ICANON | ECHO);

    // A short timeout lets a lone Escape key expire instead of making the
    // editor block forever while waiting for the rest of a sequence.
    edit_termios.c_cc[VMIN] = 0;
    edit_termios.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &edit_termios) < 0)
    {
        return -1;
    }

    size_t length = 0;
    size_t cursor = 0;
    char draft[capacity];
    draft[0] = 0;
    size_t draft_cursor = 0;
    enum Shell_Escape_State escape_state = ESCAPE_STATE_NONE;
    unsigned int escape_parameter = 0;
    int result = -1;

    // Each input line starts after the newest history entry.
    g_shell_history.selected = &g_shell_history.list;
    buf[0] = 0;
    while (true)
    {
        unsigned char c;
        ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
        if (bytes_read < 0) break;
        if (bytes_read == 0)
        {
            escape_state = ESCAPE_STATE_NONE;
            continue;
        }

        if (escape_state == ESCAPE_STATE_ESCAPE)
        {
            if (c == '[')
            {
                escape_state = ESCAPE_STATE_CSI;
                escape_parameter = 0;
            }
            else
            {
                // Some terminals use SS3 sequences for Home and End.
                escape_state =
                    (c == 'O') ? ESCAPE_STATE_SS3 : ESCAPE_STATE_NONE;
            }
            continue;
        }
        if (escape_state == ESCAPE_STATE_CSI)
        {
            escape_state = ESCAPE_STATE_NONE;
            if (c == 'A')
            {
                // UP arrow key
                if (shell_history_peek_selection_type() == NO_ENTRY)
                {
                    // save draft
                    memcpy(draft, buf, length + 1);
                    draft_cursor = cursor;
                }
                shell_history_select_previous();
                if (shell_history_peek_selection_type() != NO_ENTRY)
                {
                    // -1 to keep room for a newline
                    length = shell_history_get_selected(buf, capacity - 1);
                    cursor = length;
                    redraw_input(prompt, buf, length, cursor);
                }
            }
            else if (c == 'B')
            {
                // DOWN arrow key
                shell_history_select_next();
                enum Shell_History_Selection selection =
                    shell_history_peek_selection_type();
                if (selection == NO_ENTRY)
                {
                    // restore draft
                    length = strlen(draft);
                    memcpy(buf, draft, length + 1);
                    cursor = min(draft_cursor, length);
                    redraw_input(prompt, buf, length, cursor);
                }
                else
                {
                    length = shell_history_get_selected(buf, capacity - 1);
                    cursor = length;
                    redraw_input(prompt, buf, length, cursor);
                }
            }
            else if (c == 'C')
            {
                // right arrow
                if (cursor < length) cursor++;
                redraw_input(prompt, buf, length, cursor);
            }
            else if (c == 'D')
            {
                // left arrow
                if (cursor > 0) cursor--;
                redraw_input(prompt, buf, length, cursor);
            }
            else if (c == 'H')
            {
                // home key
                cursor = 0;
                redraw_input(prompt, buf, length, cursor);
            }
            else if (c == 'F')
            {
                // end key
                cursor = length;
                redraw_input(prompt, buf, length, cursor);
            }
            else if ((c >= '0') && (c <= '9'))
            {
                escape_parameter = (unsigned int)(c - '0');
                escape_state = ESCAPE_STATE_CSI_PARAMETER;
            }
            continue;
        }
        if (escape_state == ESCAPE_STATE_CSI_PARAMETER)
        {
            if ((c >= '0') && (c <= '9'))
            {
                escape_parameter =
                    (escape_parameter * 10) + (unsigned int)(c - '0');
                continue;
            }

            escape_state = ESCAPE_STATE_NONE;
            if ((c == '~') &&
                ((escape_parameter == 1) || (escape_parameter == 7)))
            {
                cursor = 0;
                redraw_input(prompt, buf, length, cursor);
            }
            else if ((c == '~') &&
                     ((escape_parameter == 4) || (escape_parameter == 8)))
            {
                cursor = length;
                redraw_input(prompt, buf, length, cursor);
            }
            continue;
        }
        if (escape_state == ESCAPE_STATE_SS3)
        {
            escape_state = ESCAPE_STATE_NONE;
            if (c == 'H')
                cursor = 0;
            else if (c == 'F')
                cursor = length;
            else
                continue;

            redraw_input(prompt, buf, length, cursor);
            continue;
        }

        if (c == '\033')
        {
            escape_state = ESCAPE_STATE_ESCAPE;
        }
        else if (c == '\r' || c == '\n')
        {
            write_buffer(STDOUT_FILENO, "\r\n", 2);
            shell_history_add(buf, length);
            buf[length++] = '\n';
            buf[length] = 0;
            result = 0;
            break;
        }
        else if ((c == '\177') || (c == '\b'))
        {
            // BACK key
            if (cursor > 0)
            {
                memmove(buf + cursor - 1, buf + cursor, length - cursor + 1);
                cursor--;
                length--;
                redraw_input(prompt, buf, length, cursor);
            }
        }
        else if (c == '\004')
        {
            if (length == 0) break;
        }
        else if ((c >= ' ') && (c != '\177') && (length + 2 < capacity))
        {
            memmove(buf + cursor + 1, buf + cursor, length - cursor + 1);
            buf[cursor++] = c;
            length++;
            redraw_input(prompt, buf, length, cursor);
        }
    }

    // reset termios
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
    return result;
}

int getcmd(char *buf, int nbuf, bool print_prompt)
{
    static const char prompt[] = "$ ";
    if (print_prompt)
    {
        if (!write_buffer(STDOUT_FILENO, prompt, sizeof(prompt) - 1)) return -1;
    }
    memset(buf, 0, nbuf);

    if (print_prompt && isatty(STDIN_FILENO))
    {
        return read_interactive_line(buf, nbuf, prompt);
    }

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
    static char buf[INPUT_BUF_SIZE];
    bool print_prompt = true;

    if (argc == 2)
    {
        close(STDIN_FILENO);
        int fd = open(argv[1], O_RDONLY);
        if (fd != STDIN_FILENO)
        {
            fprintf(stderr, "Error reading %s, %s\n", argv[1], strerror(errno));
            return 1;
        }
        print_prompt = false;
    }
    else if (argc > 2)
    {
        fprintf(stderr, "Error: usage: sh [script]\n");
        return 1;
    }

    shell_history_init();

    int status = 0;
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
            if (chdir(buf + 3) < 0)
            {
                fprintf(stderr, "cannot cd %s, %s\n", buf + 3, strerror(errno));
            }
            continue;
        }
        if (strcmp(buf, "exit\n") == 0)
        {
            shell_history_save();
            return 0;
        }
        if (strcmp(buf, "echo $?\n") == 0)
        {
            printf("%d\n", status);
            continue;
        }
        if (strcmp(buf, "pwd\n") == 0)
        {
            char *cwd = get_current_dir_name();
            if (cwd != NULL)
            {
                printf("%s\n", cwd);
                free(cwd);
            }
            else if (errno == ENOENT)
            {
                printf("UNLINKED DIR\n");
            }
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

        wait(&status);
        status = WEXITSTATUS(status);
    }
    return -1;
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
    if (cmd == NULL) return NULL;

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
        if (argc >= MAX_EXEC_ARGSS)
        {
            printf("parsing error, argc: %d (max: %d)\n", argc, MAX_EXEC_ARGSS);
            sh_panic("too many args");
        }
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
