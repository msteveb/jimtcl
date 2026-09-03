static struct termios termios;
sbuf *term_sbuf;
int term_record;
int term_winch;
int term_resized;
int xrows, xcols;
unsigned int ibuf_pos, ibuf_cnt, ibuf_sz = 128, icmd_pos;
unsigned char *ibuf, icmd[4096];
unsigned int texec, tn;

void term_init(void)
{
	struct winsize win;
	struct termios newtermios;
	char *s;
	term_winch = 0;
	term_resized++;
	sbuf_make(term_sbuf, 2048)
	tcgetattr(0, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~(ICANON | ISIG | ECHO);
	tcsetattr(0, TCSAFLUSH, &newtermios);
	if (!ioctl(0, TIOCGWINSZ, &win)) {
		xcols = win.ws_col;
		xrows = win.ws_row;
	} else {
		if ((s = getenv("LINES")))
			xrows = atoi(s);
		if ((s = getenv("COLUMNS")))
			xcols = atoi(s);
	}
	xcols = xcols ? xcols : 80;
	xrows = xrows ? xrows : 25;
}

void term_done(void)
{
	if (!term_sbuf)
		return;
	term_commit();
	sbuf_free(term_sbuf)
	tcsetattr(0, 0, &termios);
}

void term_clean(void)
{
	term_write("\x1b[2J", 4)	/* clear screen */
	term_write("\x1b[H", 3)		/* cursor topleft */
}

void term_suspend(void)
{
	if (xvis & 8)
		term_scrl()
	term_done();
	kill(0, SIGSTOP);
	term_init();
	if (xvis & 8)
		term_scrh()
}

void term_commit(void)
{
	term_write(term_sbuf->s, term_sbuf->s_n)
	sbuf_cut(term_sbuf, 0)
	term_record = 0;
}

static void term_out(char *s)
{
	if (term_record)
		sbufn_str(term_sbuf, s)
	else
		term_write(s, strlen(s))
}

void term_chr(int ch)
{
	char s[4] = {ch};
	term_out(s);
}

void term_kill(void)
{
	term_out("\33[K");
}

void term_room(int n)
{
	char cmd[64] = "\33[";
	if (!n)
		return;
	char *s = itoa(abs(n), cmd+2);
	s[0] = n < 0 ? 'M' : 'L';
	s[1] = '\0';
	term_out(cmd);
}

void term_pos(int r, int c)
{
	char buf[64] = "\r\33[", *s;
	if (r < 0) {
		memcpy(itoa(MAX(0, c), buf+3), c > 0 ? "C" : "D", 2);
		term_out(buf);
	} else {
		s = itoa(r + 1, buf+3);
		if (c > 0) {
			*s++ = ';';
			s = itoa(c + 1, s);
		}
		memcpy(s, "H", 2);
		term_out(buf+1);
	}
}

/* read s before reading from the terminal */
void term_push(char *s, unsigned int n)
{
	static unsigned int tibuf_pos;
	if (ibuf_cnt + n >= ibuf_sz || ibuf_sz - (ibuf_cnt + n) > 128) {
		ibuf_sz = ibuf_cnt + n + 128;
		ibuf = erealloc(ibuf, ibuf_sz);
	}
	if (texec) {
		if (texec == '@' && xquit > 0) {
			xquit = 0;
			tn = 0;
		} else if (tibuf_pos != ibuf_pos)
			tn = 0;
		memmove(ibuf + ibuf_pos + n + tn,
			ibuf + ibuf_pos + tn, ibuf_cnt - ibuf_pos - tn);
		memcpy(ibuf + ibuf_pos + tn, s, n);
		tn += n;
		tibuf_pos = ibuf_pos;
	} else
		memcpy(ibuf + ibuf_cnt, s, n);
	ibuf_cnt += n;
}

int term_read(int winch)
{
	static struct pollfd ufd = {STDIN_FILENO, POLLIN};
	int cw;
	if (ibuf_pos >= ibuf_cnt) {
		if (texec) {
			xquit = !xquit ? 1 : xquit;
			if (texec == '&')
				goto err;
		}
		if (term_winch && winch) {
			*ibuf = winch;	/* yield until term_winch is cleared */
			goto ret;
		}
		cw = 0;
		re:
		/* read a single input character */
		if (xquit < 0 || poll(&ufd, 1, -1) <= 0 ||
				read(STDIN_FILENO, ibuf, 1) <= 0) {
			xquit = !isatty(STDIN_FILENO) ? -1 : xquit;
			if (term_winch && winch && xquit >= 0) {
				*ibuf = winch;
				goto ret;
			} else if (term_winch != cw && !winch && xquit >= 0) {
				cw = term_winch;
				goto re;
			}
			err:
			*ibuf = 0;
		} else if (xrr > 0) {
			static char buf[2];
			buf[0] = *ibuf;
			ex_regput(xrr, buf, 1);
		}
		ret:
		ibuf_cnt = 1;
		ibuf_pos = 0;
	}
	if (icmd_pos < sizeof(icmd))
		icmd[icmd_pos++] = ibuf[ibuf_pos];
	return ibuf[ibuf_pos++];
}

/* return a static string that changes text attributes to att */
char *term_att(int att)
{
	if (att & SYN_MK)
		return "\x1b[m";
	static char buf[128] = "\x1b[";
	char *s = buf+2;
	if (att & SYN_BD)
		{*s++ = ';'; *s++ = '1';}
	if (att & SYN_IT)
		{*s++ = ';'; *s++ = '3';}
	if (att & SYN_RV)
		{*s++ = ';'; *s++ = '7';}
	if (SYN_FGSET(att)) {
		int fg = SYN_FG(att);
		*s++ = ';';
		if (fg < 8)
			s = itoa(30 + fg, s);
		else
			s = itoa(fg, (char*)memcpy(s, "38;5;", 5)+5);
	}
	if (SYN_BGSET(att)) {
		int bg = SYN_BG(att);
		*s++ = ';';
		if (bg < 8)
			s = itoa(40 + bg, s);
		else
			s = itoa(bg, (char*)memcpy(s, "48;5;", 5)+5);
	}
	s[0] = 'm';
	s[1] = '\0';
	return buf;
}

static int cmd_make(char **argv, int *ifd, int *ofd)
{
	int pid;
	int pipefds0[2] = {-1, -1};
	int pipefds1[2] = {-1, -1};
	if (ifd)
		pipe(pipefds0);
	if (ofd)
		pipe(pipefds1);
	if (!(pid = fork())) {
		if (ifd) {		/* setting up stdin */
			dup2(pipefds0[0], 0);
			close(pipefds0[1]);
			close(pipefds0[0]);
		}
		if (ofd) {		/* setting up stdout and stderr */
			dup2(pipefds1[1], 1);
			dup2(pipefds1[1], 2);
			close(pipefds1[0]);
			close(pipefds1[1]);
		}
		execvp(argv[0], argv);
		exit(1);
	}
	if (ifd)
		close(pipefds0[0]);
	if (ofd)
		close(pipefds1[1]);
	if (pid < 0) {
		if (ifd)
			close(pipefds0[1]);
		if (ofd)
			close(pipefds1[0]);
		return -1;
	}
	if (ifd)
		*ifd = pipefds0[1];
	if (ofd)
		*ofd = pipefds1[0];
	return pid;
}

char *xgetenv(char **q)
{
	char *r = NULL;
	while (*q && !r) {
		if (**q == '$')
			r = getenv(*q+1);
		else
			return *q;
		q++;
	}
	return r;
}

/* execute a command; pass in input if ibuf and process output if oproc */
sbuf *cmd_pipe(char *cmd, sbuf *ibuf, int oproc, int *status)
{
	static char *sh[] = {"$SHELL", "sh", NULL};
	struct pollfd fds[3];
	char buf[512];
	int ifd = -1, ofd = -1;
	int nw = 0;
	char *argv[5];
	argv[0] = xgetenv(sh);
	argv[1] = xish ? "-i" : argv[0];
	argv[2] = "-c";
	argv[3] = cmd;
	argv[4] = NULL;
	int pid = cmd_make(argv+!xish, ibuf ? &ifd : NULL, oproc ? &ofd : NULL);
	if (pid <= 0)
		return NULL;
	sbuf *sb;
	sbuf_make(sb, sizeof(buf)+1)
	if (!ibuf) {
		signal(SIGINT, SIG_IGN);
		term_done();
	} else if (ifd >= 0)
		fcntl(ifd, F_SETFL, fcntl(ifd, F_GETFL, 0) | O_NONBLOCK);
	fds[0].fd = ofd;
	fds[0].events = POLLIN;
	fds[1].fd = ifd;
	fds[1].events = POLLOUT;
	fds[2].fd = ibuf ? 0 : -1;
	fds[2].events = POLLIN;
	while ((fds[0].fd >= 0 || fds[1].fd >= 0) && poll(fds, 3, 200) >= 0) {
		if (fds[0].revents & POLLIN) {
			int ret = read(fds[0].fd, buf, sizeof(buf));
			if (ret > 0 && oproc == 2)
				term_write(buf, ret)
			if (ret > 0)
				sbuf_mem(sb, buf, ret)
			else {
				close(fds[0].fd);
				fds[0].fd = -1;
			}
		} else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			close(fds[0].fd);
			fds[0].fd = -1;
		}
		if (fds[1].revents & POLLOUT && ibuf) {
			int ret = write(fds[1].fd, ibuf->s + nw, ibuf->s_n - nw);
			if (ret > 0)
				nw += ret;
			if (ret <= 0 || nw == ibuf->s_n) {
				close(fds[1].fd);
				fds[1].fd = -1;
			}
		} else if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			close(fds[1].fd);
			fds[1].fd = -1;
		}
		if (fds[2].revents & POLLIN) {
			int ret = read(fds[2].fd, buf, sizeof(buf));
			for (int i = 0; i < ret; i++)
				if ((unsigned char) buf[i] == TK_CTL('c'))
					kill(pid, SIGINT);
		} else if (fds[2].revents & (POLLERR | POLLHUP | POLLNVAL))
			fds[2].fd = -1;
	}
	if (fds[0].fd >= 0)
		close(ofd);
	if (fds[1].fd >= 0)
		close(ifd);
	waitpid(pid, status, 0);
	signal(SIGTTOU, SIG_IGN);
	tcsetpgrp(STDIN_FILENO, getpgrp());
	signal(SIGTTOU, SIG_DFL);
	if (!ibuf) {
		if (term_sbuf)
			term_init();
		signal(SIGINT, SIG_DFL);
	}
	sbufn_ret(sb, sb)
}
