/* SPDX-License-Identifier: MIT */

#include <sys/ioctl.h>
#include <termios.h>

int tcgetattr(int fd, struct termios *termios_p)
{
    return ioctl(fd, TCGETA, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    return ioctl(fd, TCSETA, termios_p);
}

void cfmakeraw(struct termios *termios_p)
{
    // Ignore optional_actions
    termios_p->c_lflag = 0;
}
