/* SPDX-License-Identifier: MIT */
#pragma once

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

#define NCCS 32
struct termios
{
    tcflag_t c_iflag;  ///< input modes
    tcflag_t c_oflag;  ///< output modes
    tcflag_t c_cflag;  ///< control modes
    tcflag_t c_lflag;  ///< local modes
    cc_t c_cc[NCCS];   ///< special characters
};

// for c_iflag
#define IGNBRK 0000001  ///< Ignore break condition.
#define BRKINT 0000002  ///< Signal interrupt on break.
#define IGNPAR 0000004  ///< Ignore characters with parity errors.
#define PARMRK 0000010  ///< Mark parity and framing errors.
#define INPCK 0000020   ///< Enable input parity check.
#define ISTRIP 0000040  ///< Strip 8th bit off characters.
#define INLCR 0000100   ///< Map NL to CR on input.
#define IGNCR 0000200   ///< Ignore CR.
#define ICRNL 0000400   ///< Map CR to NL on input.
#define IUCLC \
    0001000  ///< Map uppercase characters to lowercase on input (not in POSIX).
#define IXON 0002000     ///< Enable start/stop output control.
#define IXANY 0004000    ///< Enable any character to restart output.
#define IXOFF 0010000    ///< Enable start/stop input control.
#define IMAXBEL 0020000  ///< Ring bell when input queue is full (not in POSIX).
#define IUTF8 0040000    ///< Input is UTF8 (not in POSIX).

// c_oflag
#define OPOST 0000001  ///< Post-process output.
#define OLCUC \
    0000002  ///< Map lowercase characters to uppercase on output. (not in
             ///< POSIX).
#define ONLCR 0000004   ///< Map NL to CR-NL on output.
#define OCRNL 0000010   ///< Map CR to NL on output.
#define ONOCR 0000020   ///< No CR output at column 0.
#define ONLRET 0000040  ///< NL performs CR function.
#define OFILL 0000100   ///< Use fill characters for delay.
#define OFDEL 0000200   ///< Fill is DEL.

// c_cflag
#define CSIZE 0000060
#define CS5 0000000
#define CS6 0000020
#define CS7 0000040
#define CS8 0000060
#define CSTOPB 0000100
#define CREAD 0000200
#define PARENB 0000400
#define PARODD 0001000
#define HUPCL 0002000
#define CLOCAL 0004000

// c_lflag
#define ISIG 0000001    ///< Enable signals.
#define ICANON 0000002  ///< Canonical input (erase and kill processing).
#define ECHO 0000010    ///< Enable echo of input to output
#define ECHOE 0000020   ///< Echo erase character as error-correcting backspace.
#define ECHOK 0000040   ///< Echo KILL.
#define ECHONL 0000100  ///< Echo NL.
#define NOFLSH 0000200  ///< Disable flush after interrupt or quit.
#define TOSTOP 0000400  ///< Send SIGTTOU for background output.
#define IEXTEN 0100000  ///< Enable implementation-defined input processing.

// for tcsetattr
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

// c_cc characters
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5  ///< read() timeout in 1/10s
#define VMIN 6   ///< read() blocks till this many bytes are read (at least)
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

struct winsize
{
    unsigned short int ws_row;
    unsigned short int ws_col;
    unsigned short int ws_xpixel;
    unsigned short int ws_ypixel;
};
