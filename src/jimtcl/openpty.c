/*
 * From musl:     http://www.musl-libc.org/
----------------------------------------------------------------------
Copyright © 2005-2020 Rich Felker, et al.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
----------------------------------------------------------------------
*/
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdio.h>

/* We implement our own openpty() on Linux so that we don't need to link against libutil */

int openpty(int *pm, int *ps, char name[20], const struct termios *tio, const struct winsize *ws)
{
	sig_t old_signal;
	int ret = -1;
	int m = posix_openpt(O_RDWR|O_NOCTTY);
	if (m < 0) return -1;

	old_signal = signal(SIGCHLD, SIG_DFL);
	if (grantpt(m) >= 0) {
		if (unlockpt(m) >= 0) {
			char buf[20];
			int s;

			if (!name) name = buf;
			snprintf(name, sizeof(buf), "%s", ptsname(m));
			if ((s = open(name, O_RDWR|O_NOCTTY)) >= 0) {
				if (tio) tcsetattr(s, TCSANOW, tio);
				if (ws) ioctl(s, TIOCSWINSZ, ws);

				*pm = m;
				*ps = s;

				ret = 0;

				return 0;
			}
		}
	}
	signal(SIGCHLD, old_signal);

	if (ret) {
		close(m);
	}
	return ret;
}
