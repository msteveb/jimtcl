/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Win32 support
 * - Save and load history containing newlines
 *
 * Bloat:
 * - Completion?
 *
 * List of escape sequences used by this program, we do everything just
 * a few sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * CHA (Cursor Horizontal Absolute)
 *    Sequence: ESC [ n G
 *    Effect: moves cursor to column n
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward of n chars
 *
 * The following are used to clear the screen: ESC [ H ESC [ 2 J
 * This is actually composed of two sequences:
 *
 * cursorhome
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED2 (Clear entire screen)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 * == For highlighting control characters, we also use the following two ==
 * SO (enter StandOut)
 *    Sequence: ESC [ 7 m
 *    Effect: Uses some standout mode such as reverse video
 *
 * SE (Standout End)
 *    Sequence: ESC [ 0 m
 *    Effect: Exit standout mode
 *
 * == Only used if TIOCGWINSZ fails ==
 * DSR/CPR (Report cursor position)
 *    Sequence: ESC [ 6 n
 *    Effect: reports current cursor position as ESC [ NNN ; MMM R
 */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include "linenoise.h"

#include "jim-config.h"
#ifdef JIM_UTF8
#define USE_UTF8
#endif
#include "utf8.h"

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096
static char *unsupported_term[] = {"dumb","cons25",NULL};

static struct termios orig_termios; /* in order to restore at exit */
static int rawmode = 0; /* for atexit() function to check if restore is needed*/
static int atexit_registered = 0; /* register atexit just 1 time */
static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;

static void linenoiseAtExit(void);
static int fd_read(int fd);
static void getColumns(int fd, int *cols);

static int isUnsupportedTerm(void) {
    char *term = getenv("TERM");
    int j;

    if (term == NULL) return 0;
    for (j = 0; unsupported_term[j]; j++)
        if (!strcasecmp(term,unsupported_term[j])) return 1;
    return 0;
}

static void freeHistory(void) {
    if (history) {
        int j;

        for (j = 0; j < history_len; j++)
            free(history[j]);
        free(history);
        history = NULL;
    }
}

static int enableRawMode(int fd) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSADRAIN,&raw) < 0) goto fatal;
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

static void disableRawMode(int fd) {
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd,TCSADRAIN,&orig_termios) != -1)
        rawmode = 0;
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void linenoiseAtExit(void) {
    disableRawMode(STDIN_FILENO);
    freeHistory();
}

/* Structure to contain the status of the current (being edited) line */
struct current {
    int fd;     /* Terminal fd */
    char *buf;  /* Current buffer. Always null terminated */
    int bufmax; /* Size of the buffer, including space for the null termination */
    int len;    /* Number of bytes in 'buf' */
    int chars;  /* Number of chars in 'buf' (utf-8 chars) */
    int pos;    /* Cursor position, measured in chars */
    int cols;   /* Size of the window, in chars */
    const char *prompt;
};

/* gcc/glibc insists that we care about the return code of write! */
#if defined(__GNUC__) && !defined(__clang__)
#define IGNORE_RC(EXPR) ((EXPR) < 0 ? -1 : 0)
#else
#define IGNORE_RC(EXPR) EXPR
#endif

/* This is fd_printf() on some systems, but use a different
 * name to avoid conflicts
 */
static void fd_printf(int fd, const char *format, ...)
{
    va_list args;
    char buf[64];
    int n;

    va_start(args, format);
    n = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    IGNORE_RC(write(fd, buf, n));
}

static int utf8_getchars(char *buf, int c)
{
#ifdef USE_UTF8
    return utf8_fromunicode(buf, c);
#else
    *buf = c;
    return 1;
#endif
}

/**
 * Returns the unicode character at the given offset,
 * or -1 if none.
 */
static int get_char(struct current *current, int pos)
{
    if (pos >= 0 && pos < current->chars) {
        int c;
        int i = utf8_index(current->buf, pos);
        (void)utf8_tounicode(current->buf + i, &c);
        return c;
    }
    return -1;
}

static void refreshLine(const char *prompt, struct current *current) {
    int plen;
    int pchars;
    int backup = 0;
    int i;
    const char *buf = current->buf;
    int chars = current->chars;
    int pos = current->pos;
    int b;
    int ch;
    int n;

    /* Should intercept SIGWINCH. For now, just get the size every time */
    getColumns(current->fd, &current->cols);

    plen = strlen(prompt);
    pchars = utf8_strlen(prompt, plen);

    /* Account for a line which is too long to fit in the window.
     * Note that control chars require an extra column
     */

    /* How many cols are required to the left of 'pos'?
     * The prompt, plus one extra for each control char
     */
    n = pchars + utf8_strlen(buf, current->len);
    b = 0;
    for (i = 0; i < pos; i++) {
        b += utf8_tounicode(buf + b, &ch);
        if (ch < ' ') {
            n++;
        }
    }

    /* If too many are need, strip chars off the front of 'buf'
     * until it fits. Note that if the current char is a control character,
     * we need one extra col.
     */
    if (current->pos < current->chars && get_char(current, current->pos) < ' ') {
        n++;
    }

    while (n >= current->cols) {
        b = utf8_tounicode(buf, &ch);
        if (ch < ' ') {
            n--;
        }
        n--;
        buf += b;
        pos--;
        chars--;
    }

    /* Cursor to left edge, then the prompt */
    fd_printf(current->fd, "\x1b[0G");
    IGNORE_RC(write(current->fd, prompt, plen));

    /* Now the current buffer content */

    /* Need special handling for control characters.
     * If we hit 'cols', stop.
     */
    b = 0; /* unwritted bytes */
    n = 0; /* How many control chars were written */
    for (i = 0; i < chars; i++) {
        int ch;
        int w = utf8_tounicode(buf + b, &ch);
        if (ch < ' ') {
            n++;
        }
        if (pchars + i + n >= current->cols) {
            break;
        }
        if (ch < ' ') {
            /* A control character, so write the buffer so far */
            IGNORE_RC(write(current->fd, buf, b));
            buf += b + w;
            b = 0;
            fd_printf(current->fd, "\033[7m^%c\033[0m", ch + '@');
            if (i < pos) {
                backup++;
            }
        }
        else {
            b += w;
        }
    }
    IGNORE_RC(write(current->fd, buf, b));

    /* Erase to right, move cursor to original position */
    fd_printf(current->fd, "\x1b[0K" "\x1b[0G\x1b[%dC", pos + pchars + backup);
}

static void set_current(struct current *current, const char *str)
{
    strncpy(current->buf, str, current->bufmax);
    current->buf[current->bufmax - 1] = 0;
    current->len = strlen(current->buf);
    current->pos = current->chars = utf8_strlen(current->buf, current->len);
}

static int has_room(struct current *current, int bytes)
{
    return current->len + bytes < current->bufmax - 1;
}

/**
 * Removes the char at 'pos'.
 * 
 * Returns 1 if the line needs to be refreshed, 2 if not
 * and 0 if nothing was removed
 */
static int remove_char(struct current *current, int pos)
{
    if (pos >= 0 && pos < current->chars) {
        int p1, p2;
        int ret = 1;
        p1 = utf8_index(current->buf, pos);
        p2 = p1 + utf8_index(current->buf + p1, 1);

        /* optimise remove char in the case of removing the last char */
        if (current->pos == pos + 1 && current->pos == current->chars) {
            if (current->buf[pos] >= ' ' && utf8_strlen(current->prompt, -1) + utf8_strlen(current->buf, current->len) < current->cols - 1) {
                ret = 2;
                fd_printf(current->fd, "\b \b");
            }
        }

        /* Move the null char too */
        memmove(current->buf + p1, current->buf + p2, current->len - p2 + 1);
        current->len -= (p2 - p1);
        current->chars--;

        if (current->pos > pos) {
            current->pos--;
        }
        return ret;
    }
    return 0;
}

/**
 * Insert 'ch' at position 'pos'
 * 
 * Returns 1 if the line needs to be refreshed, 2 if not
 * and 0 if nothing was inserted (no room)
 */
static int insert_char(struct current *current, int pos, int ch)
{
    char buf[3];
    int n = utf8_getchars(buf, ch);

    if (has_room(current, n) && pos >= 0 && pos <= current->chars) {
        int p1, p2;
        int ret = 1;
        p1 = utf8_index(current->buf, pos);
        p2 = p1 + n;

        /* optimise the case where adding a single char to the end and no scrolling is needed */
        if (current->pos == pos && current->chars == pos) {
            if (ch >= ' ' && utf8_strlen(current->prompt, -1) + utf8_strlen(current->buf, current->len) < current->cols - 1) {
                IGNORE_RC(write(current->fd, buf, n));
                ret = 2;
            }
        }

        memmove(current->buf + p2, current->buf + p1, current->len - p1);
        memcpy(current->buf + p1, buf, n);
        current->len += n;

        current->chars++;
        if (current->pos >= pos) {
            current->pos++;
        }
        return ret;
    }
    return 0;
}

#ifndef NO_COMPLETION
static linenoiseCompletionCallback *completionCallback = NULL;

static void beep() {
    fprintf(stderr, "\x7");
    fflush(stderr);
}

static void freeCompletions(linenoiseCompletions *lc) {
    size_t i;
    for (i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    free(lc->cvec);
}

static int completeLine(struct current *current) {
    linenoiseCompletions lc = { 0, NULL };
    int c = 0;

    completionCallback(current->buf,&lc);
    if (lc.len == 0) {
        beep();
    } else {
        size_t stop = 0, i = 0;

        while(!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                struct current tmp = *current;
                tmp.buf = lc.cvec[i];
                tmp.pos = tmp.len = strlen(tmp.buf);
                tmp.chars = utf8_strlen(tmp.buf, tmp.len);
                refreshLine(current->prompt, &tmp);
            } else {
                refreshLine(current->prompt, current);
            }

            c = fd_read(current->fd);
            if (c < 0) {
                break;
            }

            switch(c) {
                case '\t': /* tab */
                    i = (i+1) % (lc.len+1);
                    if (i == lc.len) beep();
                    break;
                case 27: /* escape */
                    /* Re-show original buffer */
                    if (i < lc.len) {
                        refreshLine(current->prompt, current);
                    }
                    stop = 1;
                    break;
                default:
                    /* Update buffer and return */
                    if (i < lc.len) {
                        set_current(current,lc.cvec[i]);
                    }
                    stop = 1;
                    break;
            }
        }
    }

    freeCompletions(&lc);
    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn) {
    completionCallback = fn;
}

void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
    lc->cvec = realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    lc->cvec[lc->len++] = strdup(str);
}

#endif

/**
 * Returns 0 if no chars were removed or non-zero otherwise.
 */
static int remove_chars(struct current *current, int pos, int n)
{
    int removed = 0;
    while (n-- && remove_char(current, pos)) {
        removed++;
    }
    return removed;
}

/**
 * Reads a char from 'fd', waiting at most 'timeout' milliseconds.
 *
 * A timeout of -1 means to wait forever.
 *
 * Returns -1 if no char is received within the time or an error occurs.
 */
static int fd_read_char(int fd, int timeout)
{
    struct pollfd p;
    unsigned char c;

    p.fd = fd;
    p.events = POLLIN;

    if (poll(&p, 1, timeout) == 0) {
        /* timeout */
        return -1;
    }
    if (read(fd, &c, 1) != 1) {
        return -1;
    }
    return c;
}

/**
 * Reads a complete utf-8 character
 * and returns the unicode value, or -1 on error.
 */
static int fd_read(int fd)
{
#ifdef USE_UTF8
    char buf[4];
    int n;
    int i;
    int c;

    if (read(fd, &buf[0], 1) != 1) {
        return -1;
    }
    n = utf8_charlen(buf[0]);
    if (n < 1 || n > 3) {
        return -1;
    }
    for (i = 1; i < n; i++) {
        if (read(fd, &buf[i], 1) != 1) {
            return -1;
        }
    }
    buf[n] = 0;
    /* decode and return the character */
    utf8_tounicode(buf, &c);
    return c;
#else
    return fd_read_char(fd, -1);
#endif
}

static void getColumns(int fd, int *cols) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col != 0) {
        *cols = ws.ws_col;
        return;
    }
    /* Failed to query the window size. Perhaps we are on a serial terminal.
     * Try to query the width by sending the cursor as far to the right
     * and reading back the cursor position.
     * Note that this is only done once per call to linenoise rather than
     * every time the line is refreshed for efficiency reasons.
     */
    if (*cols == 0) {
        *cols = 80;

        /* Move cursor far right and report cursor position */
        fd_printf(fd, "\x1b[999G" "\x1b[6n");

        /* Parse the response: ESC [ rows ; cols R */
        if (fd_read_char(fd, 100) == 0x1b && fd_read_char(fd, 100) == '[') {
            int n = 0;
            while (1) {
                int ch = fd_read_char(fd, 100);
                if (ch == ';') {
                    /* Ignore rows */
                    n = 0;
                }
                else if (ch == 'R') {
                    /* Got cols */
                    if (n != 0 && n < 1000) {
                        *cols = n;
                    }
                    break;
                }
                else if (ch >= 0 && ch <= '9') {
                    n = n * 10 + ch - '0';
                }
                else {
                    break;
                }
            }
        }
    }
}

/* Use -ve numbers here to co-exist with normal unicode chars */
enum {
    SPECIAL_NONE,
    SPECIAL_UP = -20,
    SPECIAL_DOWN = -21,
    SPECIAL_LEFT = -22,
    SPECIAL_RIGHT = -23,
    SPECIAL_DELETE = -24,
    SPECIAL_HOME = -25,
    SPECIAL_END = -26,
};

/**
 * If escape (27) was received, reads subsequent
 * chars to determine if this is a known special key.
 *
 * Returns SPECIAL_NONE if unrecognised, or -1 if EOF.
 *
 * If no additional char is received within a short time,
 * 27 is returned.
 */
static int check_special(int fd)
{
    int c = fd_read_char(fd, 50);
    int c2;

    if (c < 0) {
        return 27;
    }

    c2 = fd_read_char(fd, 50);
    if (c2 < 0) {
        return c2;
    }
    if (c == '[' || c == 'O') {
        /* Potential arrow key */
        switch (c2) {
            case 'A':
                return SPECIAL_UP;
            case 'B':
                return SPECIAL_DOWN;
            case 'C':
                return SPECIAL_RIGHT;
            case 'D':
                return SPECIAL_LEFT;
            case 'F':
                return SPECIAL_END;
            case 'H':
                return SPECIAL_HOME;
        }
    }
    if (c == '[' && c2 >= '1' && c2 <= '6') {
        /* extended escape */
        int c3 = fd_read_char(fd, 50);
        if (c2 == '3' && c3 == '~') {
            /* delete char under cursor */
            return SPECIAL_DELETE;
        }
        while (c3 != -1 && c3 != '~') {
            /* .e.g \e[12~ or '\e[11;2~   discard the complete sequence */
            c3 = fd_read_char(fd, 50);
        }
    }

    return SPECIAL_NONE;
}

#define ctrl(C) ((C) - '@')

static int linenoisePrompt(struct current *current) {
    int history_index = 0;

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd("");

    set_current(current, "");
    refreshLine(current->prompt, current);

    while(1) {
        int c = fd_read(current->fd);

#ifndef NO_COMPLETION
        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == 9 && completionCallback != NULL) {
            c = completeLine(current);
            /* Return on errors */
            if (c < 0) return current->len;
            /* Read next character when 0 */
            if (c == 0) continue;
        }
#endif

process_char:
        if (c == -1) return current->len;
        switch(c) {
        case '\r':    /* enter */
            history_len--;
            free(history[history_len]);
            return current->len;
        case ctrl('C'):     /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case 127:   /* backspace */
        case ctrl('H'):
            if (remove_char(current, current->pos - 1) == 1) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('D'):     /* ctrl-d */
            if (current->len == 0) {
                /* Empty line, so EOF */
                history_len--;
                free(history[history_len]);
                return -1;
            }
            /* Otherwise delete char to right of cursor */
            if (remove_char(current, current->pos)) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('W'):    /* ctrl-w */
            /* eat any spaces on the left */
            {
                int pos = current->pos;
                while (pos > 0 && get_char(current, pos - 1) == ' ') {
                    pos--;
                }

                /* now eat any non-spaces on the left */
                while (pos > 0 && get_char(current, pos - 1) != ' ') {
                    pos--;
                }

                if (remove_chars(current, pos, current->pos - pos)) {
                    refreshLine(current->prompt, current);
                }
            }
            break;
        case ctrl('R'):    /* ctrl-r */
            {
                /* Display the reverse-i-search prompt and process chars */
                char rbuf[50];
                char rprompt[80];
                int rchars = 0;
                int rlen = 0;
                int searchpos = history_len - 1;

                rbuf[0] = 0;
                while (1) {
                    int n = 0;
                    const char *p = NULL;
                    int skipsame = 0;
                    int searchdir = -1;

                    snprintf(rprompt, sizeof(rprompt), "(reverse-i-search)'%s': ", rbuf);
                    refreshLine(rprompt, current);
                    c = fd_read(current->fd);
                    if (c == ctrl('H') || c == 127) {
                        if (rchars) {
                            int p = utf8_index(rbuf, --rchars);
                            rbuf[p] = 0;
                            rlen = strlen(rbuf);
                        }
                        continue;
                    }
                    if (c == 27) {
                        c = check_special(current->fd);
                    }
                    if (c == ctrl('P') || c == SPECIAL_UP) {
                        /* Search for the previous (earlier) match */
                        if (searchpos > 0) {
                            searchpos--;
                        }
                        skipsame = 1;
                    }
                    else if (c == ctrl('N') || c == SPECIAL_DOWN) {
                        /* Search for the next (later) match */
                        if (searchpos < history_len) {
                            searchpos++;
                        }
                        searchdir = 1;
                        skipsame = 1;
                    }
                    else if (c >= ' ') {
                        if (rlen >= (int)sizeof(rbuf) + 3) {
                            continue;
                        }

                        n = utf8_getchars(rbuf + rlen, c);
                        rlen += n;
                        rchars++;
                        rbuf[rlen] = 0;

                        /* Adding a new char resets the search location */
                        searchpos = history_len - 1;
                    }
                    else {
                        /* Exit from incremental search mode */
                        break;
                    }

                    /* Now search through the history for a match */
                    for (; searchpos >= 0 && searchpos < history_len; searchpos += searchdir) {
                        p = strstr(history[searchpos], rbuf);
                        if (p) {
                            /* Found a match */
                            if (skipsame && strcmp(history[searchpos], current->buf) == 0) {
                                /* But it is identical, so skip it */
                                continue;
                            }
                            /* Copy the matching line and set the cursor position */
                            set_current(current,history[searchpos]);
                            current->pos = utf8_strlen(history[searchpos], p - history[searchpos]);
                            break;
                        }
                    }
                    if (!p && n) {
                        /* No match, so don't add it */
                        rchars--;
                        rlen -= n;
                        rbuf[rlen] = 0;
                    }
                }
                if (c == ctrl('G') || c == ctrl('C')) {
                    /* ctrl-g terminates the search with no effect */
                    set_current(current, "");
                    c = 0;
                }
                else if (c == ctrl('J')) {
                    /* ctrl-j terminates the search leaving the buffer in place */
                    c = 0;
                }
                /* Go process the char normally */
                refreshLine(current->prompt, current);
                goto process_char;
            }
            break;
        case ctrl('T'):    /* ctrl-t */
            if (current->pos > 0 && current->pos < current->chars) {
                c = get_char(current, current->pos);
                remove_char(current, current->pos);
                insert_char(current, current->pos - 1, c);
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('V'):    /* ctrl-v */
            if (has_room(current, 3)) {
                /* Insert the ^V first */
                if (insert_char(current, current->pos, c)) {
                    refreshLine(current->prompt, current);
                    /* Now wait for the next char. Can insert anything except \0 */
                    c = fd_read(current->fd);

                    /* Remove the ^V first */
                    remove_char(current, current->pos - 1);
                    if (c != -1) {
                        /* Insert the actual char */
                        insert_char(current, current->pos, c);
                    }
                    refreshLine(current->prompt, current);
                }
            }
            break;
        case ctrl('B'):     /* ctrl-b */
        case ctrl('F'):     /* ctrl-f */
        case ctrl('P'):    /* ctrl-p */
        case ctrl('N'):    /* ctrl-n */
        case 27: {   /* escape sequence */
            int dir = -1;
            if (c == 27) {
                c = check_special(current->fd);
            }
            switch (c) {
                case ctrl('B'):
                case SPECIAL_LEFT:
                    if (current->pos > 0) {
                        current->pos--;
                        refreshLine(current->prompt, current);
                    }
                    break;
                case ctrl('F'):
                case SPECIAL_RIGHT:
                    if (current->pos < current->chars) {
                        current->pos++;
                        refreshLine(current->prompt, current);
                    }
                    break;
                case ctrl('P'):
                case SPECIAL_UP:
                    dir = 1;
                case ctrl('N'):
                case SPECIAL_DOWN:
                    if (history_len > 1) {
                        /* Update the current history entry before to
                         * overwrite it with tne next one. */
                        free(history[history_len-1-history_index]);
                        history[history_len-1-history_index] = strdup(current->buf);
                        /* Show the new entry */
                        history_index += dir;
                        if (history_index < 0) {
                            history_index = 0;
                            break;
                        } else if (history_index >= history_len) {
                            history_index = history_len-1;
                            break;
                        }
                        set_current(current, history[history_len-1-history_index]);
                        refreshLine(current->prompt, current);
                    }
                    break;

                case SPECIAL_DELETE:
                    if (remove_char(current, current->pos) == 1) {
                        refreshLine(current->prompt, current);
                    }
                    break;
                case SPECIAL_HOME:
                    current->pos = 0;
                    refreshLine(current->prompt, current);
                    break;
                case SPECIAL_END:
                    current->pos = current->chars;
                    refreshLine(current->prompt, current);
                    break;
            }
            }
            break;
        default:
            /* Only tab is allowed without ^V */
            if (c == '\t' || c >= ' ') {
                if (insert_char(current, current->pos, c) == 1) {
                    refreshLine(current->prompt, current);
                }
            }
            break;
        case ctrl('U'): /* Ctrl+u, delete to beginning of line. */
            if (remove_chars(current, 0, current->pos)) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('K'): /* Ctrl+k, delete from current to end of line. */
            if (remove_chars(current, current->pos, current->chars - current->pos)) {
                refreshLine(current->prompt, current);
            }
            break;
        case ctrl('A'): /* Ctrl+a, go to the start of the line */
            current->pos = 0;
            refreshLine(current->prompt, current);
            break;
        case ctrl('E'): /* ctrl+e, go to the end of the line */
            current->pos = current->chars;
            refreshLine(current->prompt, current);
            break;
        case ctrl('L'): /* Ctrl+L, clear screen */
            /* clear screen */
            fd_printf(current->fd, "\x1b[H\x1b[2J");
            /* Force recalc of window size for serial terminals */
            current->cols = 0;
            refreshLine(current->prompt, current);
            break;
        }
    }
    return current->len;
}

static int linenoiseRaw(char *buf, size_t buflen, const char *prompt) {
    int fd = STDIN_FILENO;
    int count;

    if (buflen == 0) {
        errno = EINVAL;
        return -1;
    }
    if (!isatty(STDIN_FILENO)) {
        if (fgets(buf, buflen, stdin) == NULL) return -1;
        count = strlen(buf);
        if (count && buf[count-1] == '\n') {
            count--;
            buf[count] = '\0';
        }
    } else {
        struct current current;

        if (enableRawMode(fd) == -1) return -1;

        current.fd = fd;
        current.buf = buf;
        current.bufmax = buflen;
        current.len = 0;
        current.chars = 0;
        current.pos = 0;
        current.cols = 0;
        current.prompt = prompt;

        count = linenoisePrompt(&current);
        disableRawMode(fd);

        printf("\n");
    }
    return count;
}

char *linenoise(const char *prompt) {
    char buf[LINENOISE_MAX_LINE];
    int count;

    if (isUnsupportedTerm()) {
        size_t len;

        printf("%s",prompt);
        fflush(stdout);
        if (fgets(buf,LINENOISE_MAX_LINE,stdin) == NULL) return NULL;
        len = strlen(buf);
        while(len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
        return strdup(buf);
    } else {
        count = linenoiseRaw(buf,LINENOISE_MAX_LINE,prompt);
        if (count == -1) return NULL;
        return strdup(buf);
    }
}

/* Using a circular buffer is smarter, but a bit more complex to handle. */
int linenoiseHistoryAdd(const char *line) {
    char *linecopy;

    if (history_max_len == 0) return 0;
    if (history == NULL) {
        history = malloc(sizeof(char*)*history_max_len);
        if (history == NULL) return 0;
        memset(history,0,(sizeof(char*)*history_max_len));
    }
    linecopy = strdup(line);
    if (!linecopy) return 0;
    if (history_len == history_max_len) {
        free(history[0]);
        memmove(history,history+1,sizeof(char*)*(history_max_len-1));
        history_len--;
    }
    history[history_len] = linecopy;
    history_len++;
    return 1;
}

int linenoiseHistorySetMaxLen(int len) {
    char **new;

    if (len < 1) return 0;
    if (history) {
        int tocopy = history_len;

        new = malloc(sizeof(char*)*len);
        if (new == NULL) return 0;
        if (len < tocopy) tocopy = len;
        memcpy(new,history+(history_max_len-tocopy), sizeof(char*)*tocopy);
        free(history);
        history = new;
    }
    history_max_len = len;
    if (history_len > history_max_len)
        history_len = history_max_len;
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(char *filename) {
    FILE *fp = fopen(filename,"w");
    int j;

    if (fp == NULL) return -1;
    for (j = 0; j < history_len; j++) {
        const char *str = history[j];
        /* Need to encode backslash, nl and cr */
        while (*str) {
            if (*str == '\\') {
                fputs("\\\\", fp);
            }
            else if (*str == '\n') {
                fputs("\\n", fp);
            }
            else if (*str == '\r') {
                fputs("\\r", fp);
            }
            else {
                fputc(*str, fp);
            }
            str++;
        }
        fputc('\n', fp);
    }

    fclose(fp);
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int linenoiseHistoryLoad(char *filename) {
    FILE *fp = fopen(filename,"r");
    char buf[LINENOISE_MAX_LINE];

    if (fp == NULL) return -1;

    while (fgets(buf,LINENOISE_MAX_LINE,fp) != NULL) {
        char *src, *dest;

        /* Decode backslash escaped values */
        for (src = dest = buf; *src; src++) {
            char ch = *src;

            if (ch == '\\') {
                src++;
                if (*src == 'n') {
                    ch = '\n';
                }
                else if (*src == 'r') {
                    ch = '\r';
                } else {
                    ch = *src;
                }
            }
            *dest++ = ch;
        }
        /* Remove trailing newline */
        if (dest != buf && (dest[-1] == '\n' || dest[-1] == '\r')) {
            dest--;
        }
        *dest = 0;

        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}

/* Provide access to the history buffer.
 *
 * If 'len' is not NULL, the length is stored in *len.
 */
char **linenoiseHistory(int *len) {
    if (len) {
        *len = history_len;
    }
    return history;
}
