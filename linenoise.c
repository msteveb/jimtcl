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
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Switch to gets() if $TERM is something we can't support.
 * - Win32 support
 *
 * Bloat:
 * - Completion?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
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
 * For highlighting control characters, we also use:
 * SO (enter StandOut)
 *    Sequence: ESC [ 7 m
 *    Effect: Uses some standout mode such as reverse video
 *
 * SE (Standout End)
 *    Sequence: ESC [ 0 m
 *    Effect: Exit standout mode
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
#include <unistd.h>

#include "linenoise.h"

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
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

static void disableRawMode(int fd) {
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd,TCSAFLUSH,&orig_termios) != -1)
        rawmode = 0;
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void linenoiseAtExit(void) {
    disableRawMode(STDIN_FILENO);
    freeHistory();
}

static int getColumns(void) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1) return 80;
    return ws.ws_col;
}

/* Structure to contain the status of the current (being edited) line */
struct current {
    int fd;     /* Terminal fd */
    char *buf;  /* Current buffer. Always null terminated */
    int bufmax; /* Size of the buffer, including space for the null termination */
    int len;    /* Number of bytes in 'buf' */
    int pos;    /* Cursor position, measured in chars */
    int cols;   /* Size of the window, in chars */
};

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
    write(fd, buf, n);
}

static void refreshLine(const char *prompt, struct current *c) {
    size_t plen = strlen(prompt);
    int extra = 0;
    size_t i, p;
    const char *buf = c->buf;
    int len = c->len;
    int pos = c->pos;

    //fprintf(stderr, "\nrefreshLine: prompt=<<%s>>, buf=<<%s>>\n", prompt, c->buf);
    //fprintf(stderr, "pos=%d, len=%d, cols=%d\n", pos, len, c->cols);
    
    while((plen+pos) >= c->cols) {
        buf++;
        len--;
        pos--;
    }
    while (plen+len > c->cols) {
        len--;
    }

    /* Cursor to left edge, then the prompt */
    fd_printf(c->fd, "\x1b[0G");
    write(c->fd, prompt, strlen(prompt));

    /* Now the current buffer content */
    /* Need special handling for control characters */
    p = 0;
    for (i = 0; i < len; i++) {
        if (buf[i] >= 0 && buf[i] < ' ') {
            write(c->fd, buf + p, i - p);
            p = i + 1;
            fd_printf(c->fd, "\033[7m^%c\033[0m", buf[i] + '@');
            if (i < pos) {
                extra++;
            }
        }
    }
    write(c->fd, buf + p, i - p);

    /* Erase to right, move cursor to original position */
    fd_printf(c->fd, "\x1b[0K" "\x1b[0G\x1b[%dC", (int)(pos+plen+extra));
}

static void set_current(struct current *current, const char *str)
{
    strncpy(current->buf, str, current->bufmax);
    current->buf[current->bufmax - 1] = 0;
    current->len = strlen(current->buf);
    current->pos = current->len;
}
                
static int has_room(struct current *current, int chars)
{
    return current->len + chars < current->bufmax - 1;
}
                
static int remove_char(struct current *current, int pos)
{
    //fprintf(stderr, "Trying to remove char at %d (pos=%d, len=%d)\n", pos, current->pos, current->len);
    if (pos >= 0 && pos < current->len) {
        if (current->pos > pos) {
            current->pos--;
        }
        /* Move the null char too */
        memmove(current->buf + pos, current->buf + pos + 1, current->len - pos);
        current->len--;
        return 1;
    }
    return 0;
}

static int insert_char(struct current *current, int pos, int ch)
{
    if (has_room(current, 1) && pos >= 0 && pos <= current->len) {
        memmove(current->buf+pos+1, current->buf + pos, current->len - pos);
        current->buf[pos] = ch;
        current->len++;
        if (current->pos >= pos) {
            current->pos++;
        }
        return 1;
    }
    return 0;
}

/* XXX: Optimise this later */
static int remove_chars(struct current *current, int pos, int n)
{
    int removed = 0;
    while (n-- && remove_char(current, pos)) {
        removed++;
    }
    return removed;
}

static int fd_read(int fd)
{
    unsigned char c;
    if (read(fd, &c, 1) != 1) {
        return -1;
    }
    return c;
}

#ifndef ctrl
#define ctrl(C) ((C) - '@')
#endif

static int linenoisePrompt(const char *prompt, struct current *current) {
    size_t plen = strlen(prompt);
    int history_index = 0;

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd("");

    set_current(current, "");
    refreshLine(prompt, current);
    
    while(1) {
        int ext;
        int c;
        int c2;

        c = fd_read(current->fd);
process_char:
        if (c < 0) return current->len;
        switch(c) {
        case ctrl('D'):     /* ctrl-d */
        case '\r':    /* enter */
            history_len--;
            free(history[history_len]);
            if (current->len == 0 && c == ctrl('D')) {
                return -1;
            }
            return current->len;

        case ctrl('C'):     /* ctrl-c */
            errno = EAGAIN;
            return -1;
        case 127:   /* backspace */
        case ctrl('H'):
            if (remove_char(current, current->pos - 1)) {
                refreshLine(prompt, current);
            }
            break;
        case ctrl('W'):    /* ctrl-w */
            /* eat any spaces on the left */
            {
                int pos = current->pos;
                while (pos > 0 && current->buf[pos - 1] == ' ') {
                    pos--;
                }

                /* now eat any non-spaces on the left */
                while (pos > 0 && current->buf[pos - 1] != ' ') {
                    pos--;
                }

                if (remove_chars(current, pos, current->pos - pos)) {
                    refreshLine(prompt, current);
                }
            }
            break;
        case ctrl('R'):    /* ctrl-r */
            {
                /* Display the reverse-i-search prompt and process chars */
                char rbuf[50];
                char rprompt[80];
                int i = 0;
                rbuf[0] = 0;
                while (1) {
                    snprintf(rprompt, sizeof(rprompt), "(reverse-i-search)'%s': ", rbuf);
                    refreshLine(rprompt, current);
                    c = fd_read(current->fd);
                    if (c == ctrl('H') || c == 127) {
                        if (i > 0) {
                            rbuf[--i] = 0;
                        }
                        continue;
                    }
                    if (c >= ' ' && c <= '~') {
                        if (i < (int)sizeof(rbuf)) {
                            int j;
                            const char *p = NULL;
                            rbuf[i++] = c;
                            rbuf[i] = 0;
                            /* Now search back through the history for a match */
                            for (j = history_len - 1; j > 0; j--) {
                                p = strstr(history[j], rbuf);
                                if (p) {
                                    /* Found a match. Copy it */
                                    set_current(current,history[j]);
                                    current->pos = p - history[j];
                                    break;
                                }
                            }
                            if (!p) {
                                /* No match, so don't add it */
                                rbuf[--i] = 0;
                            }
                        }
                        continue;
                    }
                    break;
                }
                if (c == ctrl('G')) {
                    /* ctrl-g terminates the search with no effect */
                    set_current(current, "");
                    c = 0;
                }
                else if (c == ctrl('J')) {
                    /* ctrl-j terminates the search leaving the buffer in place */
                    c = 0;
                }
                /* Go process the char normally */
                refreshLine(prompt, current);
                goto process_char;
            }
            break;
        case ctrl('T'):    /* ctrl-t */
            if (current->pos > 0 && current->pos < current->len) {
                int aux = current->buf[current->pos-1];
                current->buf[current->pos-1] = current->buf[current->pos];
                current->buf[current->pos] = aux;
                if (current->pos != current->len-1) current->pos++;
                refreshLine(prompt, current);
            }
            break;
        case ctrl('V'):    /* ctrl-v */
            if (has_room(current, 1)) {
            /* Insert the ^V first */
            if (insert_char(current, current->pos, c)) {
                refreshLine(prompt, current);
                /* Now wait for the next char. Can insert anything except \0 */
                c = fd_read(current->fd);
                if (c > 0) {
                    /* Replace the ^V with the actual char */
                    current->buf[current->pos - 1] = c;
                }
                else {
                    remove_char(current, current->pos);
                }
                refreshLine(prompt, current);
            }
            break;
        case ctrl('B'):     /* ctrl-b */
            goto left_arrow;
        case ctrl('F'):     /* ctrl-f */
            goto right_arrow;
        case ctrl('P'):    /* ctrl-p */
            c2 = 65;
            goto up_down_arrow;
        case ctrl('N'):    /* ctrl-n */
            c2 = 66;
            goto up_down_arrow;
            break;
        case 27:    /* escape sequence */
            c = fd_read(current->fd);
            if (c <= 0) {
                break;
            }
            c2 = fd_read(current->fd);
            if (c <= 0) {
                break;
            }
            ext = (c == 91 || c == 79);
            if (ext && c2 == 68) {
left_arrow:
                /* left arrow */
                if (current->pos > 0) {
                    current->pos--;
                    refreshLine(prompt, current);
                }
            } else if (ext && c2 == 67) {
right_arrow:
                /* right arrow */
                if (current->pos < current->len) {
                    current->pos++;
                    refreshLine(prompt, current);
                }
            } else if (ext && (c2 == 65 || c2 == 66)) {
up_down_arrow:
                /* up and down arrow: history */
                if (history_len > 1) {
                    /* Update the current history entry before to
                     * overwrite it with tne next one. */
                    free(history[history_len-1-history_index]);
                    history[history_len-1-history_index] = strdup(current->buf);
                    /* Show the new entry */
                    history_index += (c2 == 65) ? 1 : -1;
                    if (history_index < 0) {
                        history_index = 0;
                        break;
                    } else if (history_index >= history_len) {
                        history_index = history_len-1;
                        break;
                    }
                    set_current(current, history[history_len-1-history_index]);
                    refreshLine(prompt, current);
                }
            } else if (c == 91 && c2 > 48 && c2 < 55) {
                /* extended escape */
                c = fd_read(current->fd);
                if (c <= 0) {
                    break;
                }
                fd_read(current->fd);
                if (c2 == 51 && c == 126) {
                    /* delete char under cursor */
                    if (remove_char(current, current->pos)) {
                        refreshLine(prompt, current);
                    }
                }
            }
            break;
        default:
            /* Note that the only control character currently permitted is tab */
            if (c == '\t' || c < 0 || c >= ' ') {
                if (insert_char(current, current->pos, c)) {
                    /* Avoid a full update of the line in the trivial case. */
                    if (current->pos == current->len && c >= ' ' && plen + current->len < current->cols) {
                        char ch = c;
                        write(current->fd, &ch, 1);
                    }
                    else {
                        refreshLine(prompt, current);
                    }
                }
            }
            break;
        case ctrl('U'): /* Ctrl+u, delete to beginning of line. */
            if (remove_chars(current, 0, current->pos)) {
                refreshLine(prompt, current);
            }
            break;
        case ctrl('K'): /* Ctrl+k, delete from current to end of line. */
            if (remove_chars(current, current->pos, current->len - current->pos)) {
                refreshLine(prompt, current);
            }
            break;
        case ctrl('A'): /* Ctrl+a, go to the start of the line */
            current->pos = 0;
            refreshLine(prompt, current);
            break;
        case ctrl('E'): /* ctrl+e, go to the end of the line */
            current->pos = current->len;
            refreshLine(prompt, current);
            break;
        }
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
        current.pos = 0;
        current.cols = getColumns();

        count = linenoisePrompt(prompt, &current);
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
