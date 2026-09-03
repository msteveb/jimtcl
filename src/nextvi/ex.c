int xleft;			/* the first visible column */
int xvis;			/* startup flags */
int xai = 1;			/* autoindent option */
int xic = 1;			/* ignorecase option */
int xhl = 1;			/* syntax highlight option */
int xhll;			/* highlight current line */
int xhlw;			/* highlight current word */
int xhlp;			/* highlight {}[]() pair */
int xhlr;			/* highlight text in reverse direction */
int xled = 1;			/* use the line editor */
int xtd = +1;			/* current text direction */
int xshape = 1;			/* perform letter shaping */
int xorder = 1;			/* change the order of characters */
int xts = 8;			/* number of spaces for tab */
int xish;			/* interactive shell */
int xgrp;			/* regex search group */
int xpac;			/* print autocomplete options */
int xmpt;			/* whether to prompt after printing > 1 lines in vi */
int xpr;			/* ex_cprint register */
int xlim = -1;			/* rendering cutoff for non cursor lines */
int xseq = 1;			/* undo/redo sequence */
int xerr = 1;			/* error handling -
				bit 1: print errors, bit 2: early return, bit 3: ignore errors */
int xfr;			/* ec_find register */
int xrr;			/* record register */

int xquit;			/* exit if positive, force quit or unwind if negative */
int xrow, xoff, xtop;		/* current row, column, and top row */
int xbufcur;			/* number of active buffers */
int xgrec;			/* global vi/ex recursion depth */
int xkmap;			/* the current keymap */
int xkmap_alt = 1;		/* the alternate keymap */
int xkwddir;			/* the last search direction */
int xkwdcnt;			/* number of search kwd changes */
int xpln;			/* tracks newline from ex print and pipe stdout */
int xsep = ':';			/* ex command separator */
int xesc = '\\';		/* ex command arg escape character */
int xexec_dep;			/* ex_exec recursion depth */
sbuf *xacreg;			/* autocomplete db filter regex */
rset *xkwdrs;			/* the last searched keyword rset */
sbuf **xregs;			/* string registers */
int xregs_n;			/* allocated register count */
int xdefreg;			/* ex default register */
struct buf *bufs;		/* main buffers */
struct buf tempbufs[3];		/* temporary buffers, for internal use */
struct buf *ex_buf;		/* current buffer */
struct buf *ex_pbuf;		/* prev buffer */
static struct buf *ex_tpbuf;	/* temp prev buffer */
static int xbufsmax;		/* number of buffers */
static int xbufsalloc = 10;	/* initial number of buffers */
static int xgdep;		/* global command recursion depth */
static int xexp = '%';		/* ex command internal state expand  */
static int xexe = '!';		/* ex command external command expand */
static char xuerr[] = "unreported error";
static char xserr[] = "syntax error";
static char xgerr[] = "invalid grp";
static char xirerr[] = "invalid range";
static char xrnferr[] = "range not found";
static char *xrerr;
static void *xpret;		/* previous ex command return value */
static sbuf *xanchor;		/* anchored error status buffer */
static int xqprop;		/* number of ex_exec levels :q propagates */

/* parity rule: delim halves escapes, odd keeps delim literal */
#define ex_parity(p, sb, esc, dtest) \
int n = 0, keep, d; \
for (; p[n] == esc; n++); \
keep = n; \
d = dtest; \
if (d) \
	n -= n / 2; \
sbuf_mem(sb, p, n) \
if (d && keep & 1) \
	sb->s[sb->s_n - 1] = p[keep++]; \
p += keep; \

/* read a sub expression enclosed in a delimiter */
static void ex_sread(sbuf *sb, char **src, int delim, int esc)
{
	char *s = *src;
	while (*s && *s != delim) {
		if (*s == esc) {
			ex_parity(s, sb, esc, s[n] == delim)
			continue;
		}
		sbuf_chr(sb, *s++)
	}
	*src = *s ? s + 1 : s;
	sbuf_nul(sb)
}

/* allocate & read a sub expression enclosed in a delimiter */
static char *ex_se_read(char **src, int delim, int esc)
{
	sbuf_smake(sb, 256)
	ex_sread(sb, src, delim, esc);
	return sb->s;
}

/* allocate & read a regular expression enclosed in a delimiter */
static char *ex_re_read(char **src)
{
	int delim = **src;
	if (!delim)
		return NULL;
	++*src;
	return ex_se_read(src, delim, '\\');
}

static int rstrcmp(const char *s1, const char *s2, int l1, int l2)
{
	if (l1 != l2 || !l1)
		return 1;
	for (int i = l1-1; i >= 0; i--)
		if (s1[i] != s2[i])
			return 1;
	return 0;
}

static int bufs_find(const char *path, int len)
{
	for (int i = 0; i < xbufcur; i++)
		if (!rstrcmp(bufs[i].path, path, bufs[i].plen, len))
			return i;
	return -1;
}

static void bufs_free(int idx)
{
	free(bufs[idx].path);
	lbuf_free(bufs[idx].lb);
}

static long mtime(char *path)
{
	struct stat st;
	if (!stat(path, &st))
		return st.st_mtime;
	return -1;
}

void bufs_switch(int idx)
{
	if (ex_buf != &bufs[idx]) {
		exbuf_save(ex_buf)
		if (istempbuf(ex_buf))
			ex_pbuf = &bufs[idx] == ex_pbuf ? ex_tpbuf : ex_pbuf;
		else
			ex_pbuf = ex_buf;
		ex_buf = &bufs[idx];
	}
	exbuf_load(ex_buf)
}

static int bufs_open(const char *path, int len)
{
	int i = xbufcur;
	if (i <= xbufsmax - 1)
		xbufcur++;
	else
		bufs_free(--i);
	bufs[i].path = uc_dup(path);
	bufs[i].lb = lbuf_make();
	bufs[i].plen = len;
	bufs[i].row = 0;
	bufs[i].off = 0;
	bufs[i].top = 0;
	bufs[i].td = +1;
	bufs[i].mtime = -1;
	return i;
}

void temp_open(int i, char *name, char *ft)
{
	tempbufs[i].path = uc_dup(name);
	tempbufs[i].lb = lbuf_make();
	tempbufs[i].row = 0;
	tempbufs[i].off = 0;
	tempbufs[i].top = 0;
	tempbufs[i].td = +1;
	tempbufs[i].mtime = -1;
	tempbufs[i].ft = ft;
}

void temp_pos(int i, int row, int off, int top)
{
	if (row < 0)
		row = lbuf_len(tempbufs[i].lb)-1;
	tempbufs[i].row = row < 0 ? 0 : row;
	tempbufs[i].off = off;
	tempbufs[i].top = top;
}

void temp_switch(int i, int swap)
{
	if (ex_buf == &tempbufs[i]) {
		if (swap) {
			exbuf_save(ex_buf)
			ex_buf = ex_pbuf;
			ex_pbuf = ex_tpbuf;
		}
	} else {
		if (!istempbuf(ex_buf)) {
			ex_tpbuf = ex_pbuf;
			ex_pbuf = ex_buf;
		}
		exbuf_save(ex_buf)
		ex_buf = &tempbufs[i];
	}
	exbuf_load(ex_buf)
	syn_setft(xb_ft);
}

void temp_write(int i, char *str)
{
	if (!*str)
		return;
	struct lbuf *lb = tempbufs[i].lb;
	if (lbuf_get(lb, tempbufs[i].row))
		tempbufs[i].row++;
	lbuf_edit(lb, str, tempbufs[i].row, tempbufs[i].row, 0, 0);
}

/* set the current search keyword rset if the kwd or flags changed */
void ex_krsset(char *kwd, int dir)
{
	sbuf *reg = ex_regget('/');
	if (kwd && *kwd && ((!reg || !xkwdrs || strcmp(kwd, reg->s))
			|| ((xkwdrs->regex->flg & REG_ICASE) != xic))) {
		rset_free(xkwdrs);
		xkwdrs = rset_smake(kwd, xic ? REG_ICASE : 0);
		xkwdcnt++;
		ex_regput('/', kwd, 0);
		xkwddir = dir;
	}
	if (dir == -2 || dir == 2)
		xkwddir = dir / 2;
}

static int ex_range(char *ploc, char **num, int n, int *row)
{
	int dir, off, beg, end;
	switch (**num) {
	case '.':
		++*num;
		break;
	case '%':
		if (ploc != *num)
			break;
	case '$':
		n = row ? lbuf_eol(xb, *row, 2) : lbuf_len(xb) - 1;
		++*num;
		break;
	case '\'':
		if (!uc_isdigit(*++(*num)))
			return -2;
		for (off = 0; uc_isdigit(**num); ++*num)
			off = off * 10 + (**num - '0');
		if (lbuf_jump(xb, off, &n, row ? &n : &dir))
			return -2;
		break;
	case '>':
	case '<':
		dir = **num == '>' ? 2 : -2;
		off = row ? n : 0;
		beg = row ? *row : n + (dir > 0);
		end = row ? beg+1 : lbuf_len(xb);
		if (off < 0 || beg < 0 || beg >= lbuf_len(xb))
			return -2;
		char *e = ex_re_read(num);
		ex_krsset(e, dir);
		free(e);
		if (!xkwdrs) {
			xrerr = xserr;
			return -2;
		} else if (xgrp >= xkwdrs->nsubc) {
			xrerr = xgerr;
			return -2;
		}
		if (lbuf_search(xb, xkwdrs, xkwddir, row ? beg : 0, end,
				MIN(dir, 0), !row, &beg, &off)) {
			xrerr = xrnferr;
			return -2;
		}
		n = row ? off : beg;
		break;
	default:
		if (uc_isdigit(**num)) {
			n = atoi(*num);
			while (uc_isdigit(**num))
				++*num;
		}
	}
	while (**num) {
		dir = atoi(*num+1);
		if (**num == '-')
			n -= dir;
		else if (**num == '+')
			n += dir;
		else if (**num == '*')
			n *= dir;
		else if (**num == '/' && dir)
			n /= dir;
		else if (**num == '%' && dir)
			n %= dir;
		else
			break;
		for (++*num; uc_isdigit(**num);)
			++*num;
	}
	return n;
}

/* parse ex command addresses */
#define ex_vregion(loc, beg, end) ex_region(loc, beg, end, &xoff, &xoff)
static int ex_region(char *loc, int *beg, int *end, int *o1, int *o2)
{
	int vaddr = *loc == '%', haddr = 0, update = 0;
	int row = xrow, ooff = xoff, ret = 1, adj = 0;
	char *ploc = loc, *cmd = NULL;
	xrerr = xirerr;
	if (vaddr)
		*beg = 0;
	while (*loc) {
		if (*loc == '|') {
			loc++;
			cmd = ex_se_read(&loc, '|', xesc);
			void *err = ex_exec(cmd);
			free(cmd);
			if (err) {
				xrerr = "subcommand error";
				return 1;
			}
			continue;
		} else if (*loc == ';') {
			update = loc[1] == '#';
			loc += 1 + update;
			if ((ooff = ex_range(ploc, &loc, update ? ooff : xoff, &row)) < 0)
				return 1;
			if (haddr++ % 2)
				*o2 = ooff;
			else
				*o1 = ooff;
		} else {
			if (*loc == ',') {
				update = loc[1] == '#';
				loc += 1 + update;
			}
			adj = uc_isdigit(*loc);
			row = ex_range(ploc, &loc, update ? row : xrow, NULL);
			if (vaddr++ % 2)
				*end = row + 1 - adj;
			else
				*beg = row - adj;
		}
		while (*loc && *loc != '|' && *loc != ';' && *loc != ',')
		        loc++;
	}
	if (!vaddr) {
		*beg = xrow;
		*end = MIN(lbuf_len(xb), *beg + 1);
		ret += cmd && !haddr;
	} else if (vaddr == 1) {
		*end = *beg + 1;
		ret += adj << 1;
	}
	return (*beg < 0 || *beg >= lbuf_len(xb) ||
		*end <= *beg || *end > lbuf_len(xb)) * ret;
}

static int ex_read(sbuf *sb, char *msg, ins_state *is, int ps, int flg)
{
	int n = sb->s_n, key;
	if (xvis & 1) {
		while ((key = term_read(0)) != '\n') {
			sbuf_chr(sb, key)
			if (flg & 2 || xquit)
				break;
		}
		sbuf_nul(sb)
		return key;
	}
	sbuf_str(sb, msg)
	key = led_prompt(sb, NULL, &xkmap, is, ps, flg);
	if (key == '\n' && (!*msg || strcmp(sb->s + n, msg)))
		term_chr('\n');
	return key;
}

#define readfile(errchk) \
fd = open(xb_path, O_RDONLY); \
if (fd >= 0) { \
	errchk lbuf_rd(xb, fd, 0, lbuf_len(xb)); \
	close(fd); \
} \

int ex_edit(const char *path, int len)
{
	int fd;
	if (path[0] == '.' && path[1] == '/') {
		path += 2;
		len -= 2;
	}
	if (path[0] && ((fd = bufs_find(path, len)) >= 0)) {
		bufs_switch(fd);
		return 1;
	}
	bufs_switch(bufs_open(path, len));
	readfile()
	return 0;
}

static void *ec_edit(char *loc, char *cmd, char *arg)
{
	char msg[512];
	int fd, len, rd = 0, cd = 0;
	if (arg[0] == '.' && arg[1] == '/')
		cd = 2;
	len = strlen(arg+cd);
	if (len && ((fd = bufs_find(arg+cd, len)) >= 0)) {
		bufs_switchwft(fd)
		return NULL;
	} else if (xbufcur == xbufsmax && !strchr(cmd, '!') &&
			bufs[xbufsmax - 1].lb->modified) {
		return "last buffer modified";
	} else if (len || !xbufcur || !strchr(cmd, '!')) {
		bufs_switch(bufs_open(arg+cd, len));
		cd = 3; /* XXX: quick hack to indicate new lbuf */
	}
	readfile(rd =)
	if (cd == 3 || (!rd && fd >= 0)) {
		ex_bufpostfix(ex_buf, arg[0]);
		syn_setft(xb_ft);
	}
	snprintf(msg, sizeof(msg), "\"%s\" %dL [%c]",
			*xb_path ? xb_path : "unnamed", lbuf_len(xb),
			fd < 0 || rd ? 'f' : 'r');
	if (!(xvis & 4))
		ex_print(msg, bar_ft)
	return (fd < 0 || rd) && *arg ? xuerr : NULL;
}

static void *ec_fuzz(char *loc, char *cmd, char *arg)
{
	rset *rs;
	char *path, *p, buf[128], trunc[128], *sret = NULL;
	int c, pos, subs[2], inst = -1, lnum = -1;
	int beg, end, max = INT_MAX, dwid1, dwid2;
	int flg = REG_NEWLINE | REG_NOCAP;
	int pflg = ((xvis & 2) == 0) * 2;
	ins_state is;
	ins_init(is)
	if (*cmd !='f')
		temp_switch(1, 0);
	if (!*loc || ex_vregion(loc, &beg, &end)) {
		end = lbuf_len(xb);
		if (!end || *loc) {
			if (*cmd !='f')
				temp_switch(1, 1);
			return *loc ? xrerr : xirerr;
		}
		beg = 0;
		max = xrows ? xrows * 3 : end;
	}
	snprintf(trunc, sizeof(trunc), "truncated to %d lines", max);
	dwid1 = itoalen(max - 1);
	sbuf_smake(sb, 128)
	sbuf_smake(fuzz, 16)
	sbuf_smake(cmdbuf, 16)
	sbuf_str(fuzz, arg)
	syn_setft(fuzz_ft);
	while (1) {
		sbuf_nul(fuzz)
		c = 0;
		rs = rset_smake(fuzz->s, xic ? flg | REG_ICASE : flg);
		if (rs) {
			syn_reloadft(syn_addhl(fuzz->s, 1), rs->regex->flg);
			term_record = !!term_sbuf;
			end = MIN(end, lbuf_len(xb));
			dwid2 = itoalen(end);
			dwid1 = max == INT_MAX ? dwid2 : MIN(dwid1, dwid2);
			for (pos = beg; c < max && pos < end; pos++) {
				path = xb->ln[pos];
				if (rset_match(rs, path, 0)) {
					sbuf_mem(sb, &pos, sizeof(pos))
					p = itoa(c++, buf);
					int z, wid = p - buf;
					for (z = dwid1 + 1 - wid; z; z--)
						*p++ = ' ';
					wid = itoalen(pos+1);
					for (z = dwid2 - wid; z; z--)
						*p++ = ' ';
					p = itoa(pos+1, p);
					ex_cprint2(buf, msg_ft, -1, 0, 0, pflg)
					ex_cprint2(path, NULL, -1, (p - buf) + 1, 0, !pflg)
				}
			}
			if (c == max && c != end)
				ex_cprint2(trunc, msg_ft, -1, 0, 0, 2)
			if (pflg && c)
				term_chr('\n');
			if (term_record)
				term_commit();
		}
		if ((inst = ex_read(fuzz, "", &is, 0, 2)) == '\n' && c) {
			if (c == 1)
				break;
			if ((inst = ex_read(cmdbuf, "", NULL, 0, 0)) == '\n') {
				inst = atoi(cmdbuf->s);
				break;
			}
		}
		if (TK_INT(inst))
			goto ret;
		if (c && c < 11 && uc_isdigit(inst)) {
			inst -= '0';
			if (inst < c) {
				fuzz->s_n--;
				break;
			}
		}
		rset_free(rs);
		sbuf_cut(sb, 0)
		if (pflg) {
			term_clean();
			term_pos(xrows, 0);
		} else if (c)
			ex_print("", NULL)
	}
	if ((inst >= 0 && inst < c) || c == 1)
		lnum = *((int*)sb->s + (c == 1 ? 0 : inst));
	ret:
	syn_setft(xb_ft);
	if (fuzz->s_n > 0) {
		sbuf_cut(cmdbuf, 0)
		sbuf_str(cmdbuf, loc)
		sbuf_str(cmdbuf, cmd)
		sbuf_chr(cmdbuf, ' ')
		sbufn_mem(cmdbuf, fuzz->s, fuzz->s_n)
		lbuf_dedup(tempbufs[0].lb, cmdbuf->s, cmdbuf->s_n)
		temp_pos(0, -1, 0, 0);
		temp_write(0, cmdbuf->s);
	}
	free(cmdbuf->s);
	free(fuzz->s);
	free(sb->s);
	path = lbuf_get(xb, lnum);
	if (*cmd == 'f' && path) {
		rset_find(rs, path, subs, 0);
		xrow = lnum;
		xoff = uc_off(path, subs[0]);
	} else if (path) {
		path[lbuf_s(path)->len] = '\0';
		sret = ec_edit(loc, cmd, path);
		path[lbuf_s(path)->len] = '\n';
	} else if (*cmd != 'f')
		temp_switch(1, 1);
	rset_free(rs);
	return sret;
}

static void *ec_find(char *loc, char *cmd, char *arg)
{
	int e, pskip, nskip, dir, off, nbeg, beg, end, o1 = 0, o2 = -1;
	e = ex_region(loc, &beg, &end, &o1, &o2);
	if (e && (!xfr || (*loc && e != 2)))
		return xrerr;
	dir = cmd[1] == '+' || cmd[1] == '>' ? 2 : -2;
	if (xfr) {
		if (dir < 0)
			return "register search is forward only";
		if (cmd[1] == '+' && (!*loc || e == 2))
			return "cannot increment without range";
	}
	ex_krsset(arg, dir);
	if (!xkwdrs)
		return xserr;
	else if (xgrp >= xkwdrs->nsubc)
		return xgerr;
	if (xfr) {
		int offs[xkwdrs->nsubc];
		sbuf *sb = ex_regget(xfr);
		if (!sb)
			return "uninitialized register";
		if (!*loc || e == 2) {
			if (rset_find(xkwdrs, sb->s, offs, 0) < 0 || offs[xgrp] < 0)
				return xuerr;
			return NULL;
		}
		int pin = xrow < beg || xrow >= end || (xrow == beg && xoff < o1)
			|| (o2 >= 0 && xrow == end - 1 && xoff > o2);
		off = pin ? 0 : lbuf_pos2off(xb, beg, o1, end - 1, o2,
				xrow, xoff + (cmd[1] == '+'));
		if (off < 0 || off >= sb->s_n
				|| rset_find(xkwdrs, sb->s + off, offs, 0) < 0
				|| offs[xgrp] < 0
				|| lbuf_off2pos(xb, beg, o1, end - 1, o2,
						off + offs[xgrp], &xrow, &xoff))
			return xuerr;
		return NULL;
	}
	off = xoff;
	if (xrow < beg || xrow >= end) {
		off = 0;
		end--;
		nbeg = dir > 0 ? beg : end;
		end += dir < 0;
		pskip = -1;
		nskip = 0;
	} else {
		nbeg = xrow;
		pskip = cmd[1] == '+' ? 1 : MIN(dir, 0);
		nskip = cmd[1] == '-';
	}
	if (lbuf_search(xb, xkwdrs, xkwddir, beg, end,
			pskip, nskip, &nbeg, &off))
		return xuerr;
	xrow = nbeg;
	xoff = off;
	return NULL;
}

static void *ec_buffer(char *loc, char *cmd, char *arg)
{
	int n = atoi(arg);
	if (!arg[0]) {
		char ln[512];
		for (int i = 0; i < xbufcur; i++) {
			char c = ex_buf == bufs+i ? '%' : ' ';
			c = ex_pbuf == bufs+i ? '#' : c;
			snprintf(ln, LEN(ln), "%d %c %s", i,
				c + (char)bufs[i].lb->modified, bufs[i].path);
			ex_print(ln, msg_ft)
		}
		return NULL;
	} else if (n < 0) {
		if (-n <= LEN(tempbufs)) {
			temp_switch(-n-1, 1);
			return NULL;
		}
	} else if (n < xbufcur) {
		bufs_switchwft(n)
		return NULL;
	}
	return "no such buffer";
}

static void *ec_quit(char *loc, char *cmd, char *arg)
{
	if (xexec_dep == 1 && xgrec == 1 && !strchr(cmd, '!') && xquit >= 0)
		for (int i = 0; i < xbufcur; i++)
			if (bufs[i].lb->modified)
				return "buffers modified";
	xquit = !xquit ? 1 : xquit;
	xqprop = *loc ? atoi(loc) : -1;
	/* recursion unwind: 256 increments preserve the first 8 bits
	for exit code and -257 is the offset for comparisons. */
	if (*arg)
		xquit = (abs(atoi(arg)) & 255) + 1;
	if (strchr(cmd, '!'))
		xquit = *loc ? -xqprop * 256 - 257 - (abs(xquit) - 1) : -xquit;
	return NULL;
}

void ex_bufpostfix(struct buf *p, int clear)
{
	p->mtime = mtime(p->path);
	p->ft = syn_filetype(p->path);
	lbuf_saved(p->lb, clear);
}

static void *ec_setpath(char *loc, char *cmd, char *arg)
{
	free(xb_path);
	xb_path = uc_dup(arg);
	ex_buf->plen = strlen(arg);
	return NULL;
}

static void *ec_read(char *loc, char *cmd, char *arg)
{
	sbuf obuf, *sb;
	char msg[512];
	char *path, *ret = NULL;
	int beg, end, o1 = 0, o2 = -1;
	int row = xrow, off = xoff, fd = -1;
	struct lbuf *lb = lbuf_make(), *pxb = xb;
	path = arg[0] ? arg : xb_path;
	if (arg[0] == '!') {
		if ((sb = cmd_pipe(arg + 1, NULL, 1, NULL))) {
			lbuf_edit(lb, sb->s, 0, 0, 0, 0);
			sbuf_free(sb)
		}
	} else {
		if ((fd = open(path, O_RDONLY)) < 0) {
			ret = "open failed";
			goto err;
		}
		if (lbuf_rd(lb, fd, 0, 0)) {
			ret = "read failed";
			goto err;
		}
	}
	xb = lb;
	xrow = 0;
	xoff = 0;
	if (!*loc || ex_region(loc, &beg, &end, &o1, &o2)) {
		end = lbuf_len(xb);
		if (!end || *loc) {
			ret = *loc ? xrerr : xirerr;
			goto err;
		}
		beg = 0;
	}
	lbuf_region(lb, &obuf, beg, o1, end - 1, o2);
	lbuf_edit(pxb, obuf.s, row, row, 0, 0);
	free(obuf.s);
	snprintf(msg, sizeof(msg), "\"%s\" %dL [r]", path, end - beg);
	ex_print(msg, bar_ft)
	err:
	lbuf_free(lb);
	xrow = row;
	xoff = off;
	xb = pxb;
	if (fd >= 0)
		close(fd);
	return ret;
}

static void *ex_pipeout(char *cmd, sbuf *buf)
{
	int ret = 0;
	if (!(xvis & 2) && xmpt >= 0 && !xpln) {
		term_chr('\n');
		xpln = 1;
		xmpt = 2;
	} else if (xvis & 2 && xpln == 2) {
		term_chr('\n');
		xpln = 0;
	}
	sbuf *rsb = cmd_pipe(cmd, buf, 0, &ret);
	if (!rsb)
		return "fork failed";
	sbuf_free(rsb)
	return ret ? xuerr : NULL;
}

static void *ec_write(char *loc, char *cmd, char *arg)
{
	char msg[512], *path, *ret = NULL;
	sbuf ibuf;
	int fd, quit = xquit;
	int beg, end, o1 = -1, o2 = -1;
	path = arg[0] ? arg : xb_path;
	if (cmd[0] == 'x' && !xb->modified)
		return ec_quit("", cmd, "");
	if (!*loc || (fd = ex_region(loc, &beg, &end, &o1, &o2))) {
		if (*loc && fd != 2)
			return xrerr;
		beg = 0;
		end = lbuf_len(xb);
	}
	if (cmd[0] == 'x' || (cmd[0] == 'w' && cmd[1] == 'q')) {
		int modified = xb->modified;
		xb->modified = 0;
		ret = ec_quit("", cmd, "");
		xb->modified = modified;
		if (xquit < 0)
			quit = xquit;
		swap(&quit, &xquit);
	}
	if (arg[0] == '!') {
		if (ret)
			return ret;
		lbuf_region(xb, &ibuf, beg, MAX(0, o1), end - 1, o2);
		ret = ex_pipeout(arg + 1, &ibuf);
		free(ibuf.s);
		xquit = quit;
		return ret;
	} else if (ret)
		return "other buffers modified";
	if (!strchr(cmd, '!')) {
		if (!strcmp(xb_path, path) && mtime(path) > ex_buf->mtime)
			return "write failed: file changed";
		if (arg[0] && mtime(path) >= 0)
			return "write failed: file exists";
	}
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, conf_mode);
	if (fd < 0)
		return "write failed: cannot create file";
	if (o1 >= 0) {
		lbuf_region(xb, &ibuf, beg, o1, end - 1, o2);
		o1 = write(fd, ibuf.s, ibuf.s_n);
		free(ibuf.s);
	} else
		o1 = lbuf_wr(xb, fd, beg, end);
	close(fd);
	if (o1 < 0)
		return "write failed";
	snprintf(msg, sizeof(msg), "\"%s\" %dL [w]",
			path, end - beg);
	ex_print(msg, bar_ft)
	if (strcmp(xb_path, path))
		ec_setpath(NULL, NULL, path);
	lbuf_saved(xb, 0);
	ex_buf->mtime = mtime(path);
	xquit = quit;
	return NULL;
}

static void *ec_termexec(char *loc, char *cmd, char *arg)
{
	if (*arg && term_sbuf)
		term_exec(arg, strlen(arg), cmd[0])
	return term_sbuf ? NULL : "unsupported command";
}

void ex_cprint(char *line, char *ft, int r, int c, int left, int flg)
{
	if (xpr > 0) {
		ex_regput(xpr, line, 1);
		sbuf *pr = ex_regget(xpr);
		if (flg & 1 && xpr >= 'A' && xpr <= 'Z' && pr && pr->s_n &&
				pr->s[pr->s_n-1] != '\n')
			ex_regput(xpr, "\n", 1);
	}
	if (xvis & 1) {
		term_write(line, dstrlen(line, '\n'))
		term_write("\n", 1)
		return;
	}
	syn_blockhl = -1;
	if (flg && !(xvis & 2)) {
		term_pos(xrows, 0);
		if ((!xpln && xmpt > 0) || flg == 2)
			term_chr('\n');
		xmpt += xmpt >= 0 && flg == 1;
	}
	xpln = 0;
	preserve(int, ftidx,)
	if (ft)
		syn_setft(ft);
	led_crender(line, r, c, left, left + xcols - c)
	restore(ftidx)
	if (flg && xvis & 2)
		term_chr('\n');
}

static void *ec_insert(char *loc, char *cmd, char *arg)
{
	int beg, end, o1 = -1, o2 = -1, ps = 0, key;
	sbuf _sb, *sb = &_sb;
	if (!*loc || (key = ex_region(loc, &beg, &end, &o1, &o2))) {
		if (*loc && cmd[0] != 'c' && beg == -1 && end == 0
				&& (lbuf_len(xb) || key == 3))
			beg = 0;
		else if (*loc && key != 2)
			return xrerr;
		else {
			beg = MAX(0, MIN(lbuf_len(xb), xrow));
			end = beg + 1;
		}
	}
	if (xvis & 1 && *arg) {
		sb->s = arg;
		sb->s_n = 1;
		key = 127;
	} else {
		_sbuf_make(sb, 128,)
		if (*arg)
			term_push(arg, strlen(arg));
		while (1) {
			syn_setft(msg_ft);
			if ((key = ex_read(sb, "", NULL, ps, 0)) != '\n')
				break;
			if (xvis & 1 && !strcmp(".", sb->s + ps)) {
				sb->s_n = MAX(0, sb->s_n - 2);
				break;
			}
			sbuf_chr(sb, '\n')
			ps = sb->s_n;
		}
		syn_setft(xb_ft);
		if (key == TK_CTL('c'))
			goto ret;
		if (key == 127 && sb->s_n && sb->s[sb->s_n-1] == '\n')
			sb->s_n--;
		sbuf_nul(sb)
	}
	if (cmd[0] == 'i')
		beg = end;
	if (o1 >= 0 && cmd[0] == 'c') {
		if (sb->s == arg)
			sb->s_n = strlen(arg);
		if (!sb->s_n && o2 <= o1)
			goto ret;
		char *p = lbuf_joinsb(xb, beg, end - 1, sb, &o1, &o2);
		o1 -= sb->s[0] == '\n';
		if (sb->s != arg)
			free(sb->s);
		sb->s = p;
	} else if (key != 127)
		sbufn_chr(sb, '\n')
	else if (!sb->s_n)
		goto ret;
	ps = lbuf_len(xb);
	lbuf_edit(xb, sb->s, beg, end, o1, o2);
	xrow = MIN(lbuf_len(xb) - 1, end + lbuf_len(xb) - ps - 1);
	if (o1 >= 0)
		xoff = o1;
	ret:
	if (sb->s != arg)
		free(sb->s);
	return NULL;
}

static void *ec_print(char *loc, char *cmd, char *arg)
{
	int i, beg, end, o1 = -1, o2 = -1;
	char *o, *ln;
	if (!*cmd && !*loc && *arg)
		return "unknown command";
	if (*cmd && *arg) {
		ex_print(arg, msg_ft)
		return NULL;
	}
	if ((i = ex_region(loc, &beg, &end, &o1, &o2)))
		return i == 2 && !*cmd ? NULL : xrerr;
	if (o1 >= 0)
		xoff = o2 >= 0 ? o2 : o1;
	if (!*cmd && *loc) {
		xrow = MAX(beg, end - 1);
		return NULL;
	}
	rstate = rstates+1;
	for (i = beg; i < end; i++) {
		o = NULL;
		rstate->s = o;
		ln = lbuf_get(xb, i);
		if (o1 >= 0 && o2 >= 0 && beg == end - 1)
			o = uc_sub(ln, o1, o2);
		else if (o1 >= 0 && i == beg)
			o = uc_sub(ln, o1, -1);
		else if (o2 >= 0 && i == end - 1)
			o = uc_sub(ln, 0, o2);
		else {
			ex_cprint(ln, msg_ft, -1, 0, *loc ? 0 : xleft, 1);
			continue;
		}
		ex_cprint(o, msg_ft, -1, 0, 0, 1);
		free(o);
	}
	rstate--;
	xrow = MAX(beg, end - (cmd[0] || loc[0]));
	return NULL;
}

static void *ec_delete(char *loc, char *cmd, char *arg)
{
	int beg, end, o1 = -1, o2 = -1;
	sbuf sb;
	char *p = NULL;
	if (ex_region(loc, &beg, &end, &o1, &o2))
		return xrerr;
	if (o1 >= 0) {
		sb.s = "";
		sb.s_n = 0;
		p = lbuf_joinsb(xb, beg, end - 1, &sb, &o1, &o2);
		xoff = o1;
	}
	lbuf_edit(xb, p, beg, end, o1, o2);
	free(p);
	xrow = MIN(beg, lbuf_len(xb) - !!lbuf_len(xb));
	return NULL;
}

sbuf *ex_regget(int id)
{
	return id >= 0 && id < xregs_n ? xregs[id] : NULL;
}

void ex_regput(int c, const char *s, int append)
{
	sbuf *sb;
	if (c >= xregs_n) {
		int o = xregs_n;
		xregs_n = c + 1;
		xregs = erealloc(xregs, xregs_n * sizeof(xregs[0]));
		memset(xregs + o, 0, (xregs_n - o) * sizeof(xregs[0]));
	}
	sb = xregs[c];
	if (!sb) {
		sbuf_make(sb, 64)
		xregs[c] = sb;
	}
	if (!append)
		sbuf_cut(sb, 0)
	sbuf_str(sb, s)
	sbuf_nul4(sb)
}

static void *ec_yank(char *loc, char *cmd, char *arg)
{
	int beg, end, o1 = 0, o2 = -1;
	int reg = atoi(arg);
	if (reg < 0)
		return xserr;
	if (cmd[2] == '!') {
		sbuf *sb = ex_regget(reg);
		if (!sb)
			return xuerr;
		sbuf_free(sb)
		xregs[reg] = NULL;
		return NULL;
	} else if (ex_region(loc, &beg, &end, &o1, &o2))
		return xrerr;
	sbuf sb;
	lbuf_region(xb, &sb, beg, o1, end - 1, o2);
	ex_regput(reg, sb.s, cmd[2] == '+');
	free(sb.s);
	return NULL;
}

static void *ec_put(char *loc, char *cmd, char *arg)
{
	int beg, end, i = 0, reg = xdefreg;
	sbuf *buf;
	for (; uc_isdigit(arg[i]); i++)
		reg = i ? reg * 10 + (arg[i] - '0') : arg[i] - '0';
	if (!(buf = ex_regget(reg)))
		return "uninitialized register";
	for (; arg[i] && arg[i] != '!'; i++);
	if (arg[i] == '!' && arg[i+1])
		return ex_pipeout(arg + i + 1, buf);
	int n = lbuf_len(xb), o1 = -1, o2 = -1;
	if (!*loc || (i = ex_region(loc, &beg, &end, &o1, &o2))) {
		if (*loc && i != 2 && !(beg == -1 && end == 0 && o1 < 0
				&& (lbuf_len(xb) || i == 3)))
			return xrerr;
		else if (!*loc || i == 2) {
			beg = MAX(0, MIN(lbuf_len(xb), xrow));
			end = beg + 1;
		}
	}
	if (o1 >= 0) {
		char *p = lbuf_joinsb(xb, end - 1, end - 1, buf, &o1, &o2);
		lbuf_edit(xb, p, end - 1, end, o1, o1);
		free(p);
	} else
		lbuf_edit(xb, buf->s, end, end, o1, o1);
	xrow = MIN(lbuf_len(xb) - 1, end + lbuf_len(xb) - n - 1);
	return NULL;
}

static void *ec_num(char *loc, char *cmd, char *arg)
{
	if (cmd[1] == '?') {
		ex_print(xpret ? xpret : "no error", msg_ft)
		return *arg ? xpret : NULL;
	}
	char msg[128];
	int arr[4] = {0, 0, -1, -1};
	int ret = ex_region(loc, &arr[0], &arr[1], &arr[2], &arr[3]);
	int d = (unsigned char)*arg ^ '0';
	if (ret && !((*arg && arg[1]) || (*arg && d >= 4)))
		return xrerr;
	if (d < 4)
		itoa(arr[d], msg);
	else
		sprintf(msg, "%d %d %d %d", arr[0], arr[1], arr[2], arr[3]);
	ex_print(msg, msg_ft)
	return NULL;
}

static void *ec_undoredo(char *loc, char *cmd, char *arg)
{
	int ref;
	return (cmd[0] == 'u' ? lbuf_undo : lbuf_redo)(xb, &ref, &ref) ?
		xuerr : NULL;
}

static void *ec_bufsave(char *loc, char *cmd, char *arg)
{
	lbuf_saved(xb, *arg);
	return NULL;
}

static void *ec_mark(char *loc, char *cmd, char *arg)
{
	int beg, end, o1 = xoff, o2 = xoff;
	if (ex_region(loc, &beg, &end, &o1, &o2))
		return xrerr;
	for (int i = 0; uc_isdigit(*arg); i++) {
		int mk;
		for (mk = 0; uc_isdigit(*arg); arg++)
			mk = mk * 10 + (*arg - '0');
		lbuf_mark(xb, mk, i % 2 ? end - 1 : beg, i % 2 ? o2 : o1);
		while (*arg == ' ')
			arg++;
	}
	return NULL;
}

static void *ec_substitute(char *loc, char *cmd, char *arg)
{
	int beg, end, o1 = -1, o2 = -2, flg, grp, reg = -1;
	char *pat, *rep = NULL, *_rep, *p, *err = NULL;
	char *s = arg;
	rset *rs = xkwdrs;
	int i, first = -1, last = 0;
	struct lopt *lo;
	sbuf text, *rb;
	int e = ex_region(loc, &beg, &end, &o1, &o2);
	if (e && *loc)
		return xrerr;
	pat = ex_re_read(&s);
	if (pat && (*pat || !rs))
		rs = rset_smake(pat, xic ? REG_ICASE : 0);
	if (!rs || xgrp >= rs->nsubc) {
		if (rs != xkwdrs)
			rset_free(rs);
		free(pat);
		return rs ? xgerr : xserr;
	}
	if (pat && *s) {
		s--;
		rep = ex_re_read(&s);
	}
	free(pat);
	int offs[rs->nsubc];
	char *lnb, *ln, *suf = "", *fr = NULL;
	int b1 = 0, pend, rflg = REG_NEWLINE;
	for (i = 0, flg = 0; s[i]; i++) {
		if (s[i] == 'g')
			flg |= 1;
		else if (s[i] == 'm')
			flg |= 2;
		else if (uc_isdigit(s[i]))
			reg = (reg < 0 ? 0 : reg * 10) + s[i] - '0';
		else		/* only flags may break up the register */
			reg = -1;
	}
	if (reg >= 0) {		/* the register is the whole region */
		if (*loc)
			err = "register takes no range";
		else if (!(rb = ex_regget(reg)))
			err = "uninitialized register";
		if (err)
			goto out;
		flg |= 2;
		ln = rb->s;
		rflg = 0;
		end = 1;
		i = 0;
		goto mltest;
	} else if (e) {
		err = xrerr;
		goto out;
	} else if (o1 >= 0)
		xoff = MAX(o1, o2);
	if (flg & 2) { 	/* multiline */
		lbuf_region(xb, &text, beg, MAX(o1, 0), end - 1, o2);
		ln = text.s;
		fr = ln;
		rflg = 0;
		pend = end;
		end = beg+1;
		i = beg;
		goto mltest;
	}
	for (i = beg; i < end; i++) {
		lnb = lbuf_get(xb, i), ln = lnb, suf = "";
		b1 = o1 > 0 && i == beg ? uc_chr(lnb, o1) - lnb : 0;
		if (o2 >= 0 && i == end - 1)
			suf = uc_chr(lnb, o2);
		rflg = *suf ? 0 : REG_NEWLINE;
		if (*suf) {		/* line ends at an offset */
			free(fr);	/* o1 past o2 spans to the line end */
			ln = fr = uc_sub(lnb, 0, b1 && o1 > o2 ? -1 : o2);
		}
		ln += b1;
		mltest:;
		sbuf *r = NULL;
		lnb = ln - b1;		/* start of text not yet copied */
		while (rset_find(rs, ln, offs, rflg) >= 0) {
			rflg |= REG_NOTBOL;	/* only the first search is at bol */
			if (offs[xgrp] < 0) {
				ln += offs[1] > 0 ? offs[1] : 1;
				continue;
			} else if (!r)
				sbuf_make(r, 256)
			sbuf_mem(r, lnb, ln + offs[xgrp] - lnb)
			if (rep) {
				for (_rep = rep; *_rep; _rep++) {
					if (*_rep != '\\' || !_rep[1] || !uc_isdigit(*++_rep)) {
						sbuf_chr(r, *_rep)
						continue;
					}
					grp = *_rep - '0';
					while (grp && uc_isdigit(_rep[1]) &&
						(grp * 10 + _rep[1] - '0') < (rs->nsubc >> 1))
						grp = grp * 10 + *++_rep - '0';
					grp *= 2;
					if (grp + 1 >= rs->nsubc)
						sbuf_chr(r, *_rep)
					else if (offs[grp] >= 0)
						sbuf_mem(r, ln + offs[grp], offs[grp + 1] - offs[grp])
				}
			}
			ln += offs[xgrp + 1];
			if (!offs[xgrp + 1] && *ln)	/* zero-length match */
				sbuf_chr(r, *ln++)
			lnb = ln;
			if (!*ln || !(flg & 1))
				break;
		}
		if (r) {
			sbuf_str(r, lnb)
			sbufn_str(r, suf)	/* text after the o2 offset */
			if (reg >= 0) {
				ex_regput(reg, r->s, 0);
				sbuf_free(r)
				first = 0;
				goto out;
			} else if (first < 0) {	/* undo marks */
				first = i;
				lo = lbuf_opt(xb, xrow, xoff, 0);
				lbuf_smark(xb, lo, i, MAX(o1, 0));
				lbuf_emark(xb, lo, 0, 0);
			}
			if (flg & 2) {
				p = o1 >= 0 ? lbuf_joinsb(xb, beg, pend - 1, r, &o1, &o2) : NULL;
				lbuf_edit(xb, p ? p : r->s, beg, pend, p ? o1 : 0, MAX(o2, 0));
				free(p);
				lbuf_jump(xb, ']', &last, &pend);	/* joined region end */
			} else {
				lbuf_edit(xb, r->s, i, i + 1, 0, 0);
				last = i;
			}
			sbuf_free(r)
		}
	}
	if (first >= 0) {	/* redo marks */
		lo = lbuf_opt(xb, xrow, xoff, 0);
		lbuf_smark(xb, lo, first, MAX(o1, 0));
		lbuf_emark(xb, lo, last, MAX(o2, 0));
	}
	out:
	free(fr);
	if (rs != xkwdrs)
		rset_free(rs);
	free(rep);
	return err ? err : first < 0 ? xuerr : NULL;
}

static void *ec_exec(char *loc, char *cmd, char *arg)
{
	if (!*loc)
		return ex_pipeout(arg, NULL);
	int beg, end, o1 = -1, o2 = -1, e;
	if ((e = ex_region(loc, &beg, &end, &o1, &o2))) {
		if (lbuf_len(xb) || !(e == 3 && beg == -1 && end == 0 && o1 < 0))
			return xrerr;
		beg = 0;
	}
	sbuf text;
	lbuf_region(xb, &text, beg, MAX(o1, 0), end - 1, o2);
	sbuf *rep = cmd_pipe(arg, &text, 1, NULL);
	free(text.s);
	if (!rep)
		return "fork failed";
	if (o1 >= 0) {
		char *p = lbuf_joinsb(xb, beg, end - 1, rep, &o1, &o2);
		lbuf_edit(xb, p, beg, end, o1, o2);
		free(p);
	} else
		lbuf_edit(xb, rep->s, beg, end, 0, 0);
	sbuf_free(rep)
	return NULL;
}

static void *ec_ft(char *loc, char *cmd, char *arg)
{
	int i;
	for (i = 0; *arg && i < ftslen; i++)
		if (!strcmp(fts[i].ft, arg)) {
			arg = fts[i].ft;
			break;
		}
	if (!(loc = syn_setft(*arg ? arg : xb_ft)))
		return "filetype not found";
	xb_ft = loc;
	if (!*arg)
		ex_print(xb_ft, msg_ft)
	if (led_attsb) {
		sbuf_free(led_attsb)
		led_attsb = NULL;
	}
	for (i = 1; i < 4; i++)
		syn_reloadft(syn_findhl(i), 0);
	return NULL;
}

static void *ec_cmap(char *loc, char *cmd, char *arg)
{
	if (arg[0])
		xkmap_alt = conf_kmapfind(arg);
	else
		ex_print(conf_kmap(xkmap)[0], msg_ft)
	if (arg[0] && !strchr(cmd, '!'))
		xkmap = xkmap_alt;
	return NULL;
}

static void *ec_glob(char *loc, char *cmd, char *arg)
{
	int i, beg, end, not;
	void *ret = xuerr;
	char *pat, *s = arg;
	rset *rs;
	if (!loc[0] && !xgdep)
		loc = "%";
	if (ex_vregion(loc, &beg, &end))
		return xrerr;
	not = !!strchr(cmd, '!');
	pat = ex_re_read(&s);
	if (pat && *pat)
		rs = rset_smake(pat, xic ? REG_ICASE : 0);
	else
		rs = rset_smake(ex_regget('/') ? ex_regget('/')->s : "", xic ? REG_ICASE : 0);
	free(pat);
	if (!rs)
		return xserr;
	xgdep = !xgdep ? 1 : xgdep * 2;
	for (i = beg; i < end; i++)
		lbuf_i(xb, i)->grec |= xgdep;
	for (i = beg; i < lbuf_len(xb);) {
		char *ln = lbuf_get(xb, i);
		lbuf_s(ln)->grec &= ~xgdep;
		if (rset_match(rs, ln, REG_NEWLINE) != not) {
			xrow = i;
			if ((ret = ex_exec(s)))
				break;
			i = MIN(i, xrow);
		}
		while (i < lbuf_len(xb) && !(lbuf_i(xb, i)->grec & xgdep))
			i++;
	}
	rset_free(rs);
	xgdep /= 2;
	return ret;
}

static void *ec_while(char *loc, char *cmd, char *arg)
{
	int isdq = cmd[1] == '?';
	int inv = cmd[1 + isdq] == '!';
	char *ret = NULL;
	if (isdq && *loc) {
		int id = atoi(loc);
		if (!*arg && cmd[2] != '?') {
			int err = (xpret != NULL) ^ inv;
			if (!xanchor)
				sbuf_make(xanchor, 4 * sizeof(int))
			sbuf_mem(xanchor, &id, sizeof(id))
			sbuf_mem(xanchor, &err, sizeof(err))
			return ret;
		} else if (!xanchor)
			return ret;
		int *ap = (int*)xanchor->s, n = xanchor->s_n / sizeof(int);
		int and_res = 0, or_res = 1;
		for (int i = n; i >= 2;) {
			i -= 2;
			if (ap[i] != id)
				continue;
			and_res |= ap[i + 1];
			for (; *loc && *loc != ',' && *loc != ';'; loc++);
			if (!*loc || *loc == ';') {
				 or_res &= and_res;
				 and_res = 0;
			}
			if (!*loc) {
				if (cmd[2] == '?')
					return or_res ? xuerr : NULL;
				return (or_res ^ inv) ? xuerr : ex_exec(arg);
			}
			id = atoi(++loc);
			i = n;
		}
		return ret;
	} else if (isdq) {
		ret = (xpret != NULL) ^ inv ? xuerr : NULL;
		return !ret && *arg ? ex_exec(arg) : ret;
	} else if (!*arg)
		return ret;
	int count = *loc ? (*loc == '$' ? INT_MAX : atoi(loc)) : 1;
	for (; count && !ret; count--) {
		ret = ex_exec(arg);
		ret = inv ? ret ? NULL : xuerr : ret;
	}
	return ret;
}

static void *ec_join(char *loc, char *cmd, char *arg)
{
	int beg, end, o2 = 0;
	if (ex_vregion(loc, &beg, &end))
		return xrerr;
	xrow = beg;
	return lbuf_join(xb, beg, end+1, xoff, &o2, arg[0]) ? xuerr : NULL;
}

static void *ec_setdir(char *loc, char *cmd, char *arg)
{
	static char *exdir;
	if (cmd[1] == 'p') {
		free(exdir);
		exdir = *arg ? uc_dup(arg) : NULL;
	} else if (cmd[1] == 'd')
		dir_calc(*arg ? arg : (exdir ? exdir : "."));
	return NULL;
}

static void *ec_chdir(char *loc, char *cmd, char *arg)
{
	char oldpath[4096];
	char newpath[4096];
	char *opath;
	int i, c, plen;
	oldpath[0] = '\0';
	oldpath[sizeof(oldpath)-1] = '\0';
	if (!getcwd(oldpath, sizeof(oldpath)))
		if ((opath = getenv("PWD")))
			strncpy(oldpath, opath, sizeof(oldpath)-1);
	plen = strlen(oldpath);
	i = plen == sizeof(oldpath)-1;
	if (chdir(*arg ? arg : oldpath))
		return "chdir error";
	if (!getcwd(newpath, sizeof(newpath)))
		return "getcwd error";
	setenv("PWD", newpath, 1);
	if (i)
		return "oldpath >= 4096";
	if (plen && oldpath[plen-1] != '/')
		oldpath[plen++] = '/';
	for (i = 0; i < xbufcur; i++) {
		if (!bufs[i].path[0])
			continue;
		if (bufs[i].path[0] == '/') {
			opath = bufs[i].path;
		} else {
			opath = oldpath;
			strncpy(opath+plen, bufs[i].path, sizeof(oldpath)-plen-1);
		}
		for (c = 0; opath[c] && opath[c] == newpath[c]; c++);
		if (newpath[c] || !opath[c])
			c = 0;
		else if (opath[c] == '/')
			c++;
		opath = uc_dup(opath+c);
		free(bufs[i].path);
		bufs[i].path = opath;
		bufs[i].plen = strlen(opath);
	}
	return NULL;
}

static void *ec_setincl(char *loc, char *cmd, char *arg)
{
	rset_free(fsincl);
	if (!*arg)
		fsincl = NULL;
	else if (!(fsincl = rset_smake(arg, xic ? REG_ICASE : 0)))
		return xserr;
	return NULL;
}

static void *ec_setacreg(char *loc, char *cmd, char *arg)
{
	if (xacreg)
		sbuf_free(xacreg)
	if (*arg) {
		sbuf_make(xacreg, 128)
		sbufn_str(xacreg, arg)
	} else
		xacreg = NULL;
	return NULL;
}

static void *ec_setbufsmax(char *loc, char *cmd, char *arg)
{
	int max = *arg ? atoi(arg) : xbufsalloc;
	if (max <= 0)
		return xserr;
	xbufsmax = max;
	int bufidx = ex_buf - bufs;
	int pbufidx = ex_pbuf - bufs;
	int tpbufidx = ex_tpbuf - bufs;
	int istemp = !ex_buf ? 0 : istempbuf(ex_buf);
	for (; xbufcur > xbufsmax; xbufcur--)
		bufs_free(xbufcur - 1);
	bufs = erealloc(bufs, sizeof(struct buf) * xbufsmax);
	if (!istemp)
		ex_buf = bufidx >= xbufsmax ? bufs : bufs+bufidx;
	ex_pbuf = pbufidx >= xbufsmax ? bufs : bufs+pbufidx;
	ex_tpbuf = tpbufidx >= xbufsmax ? bufs : bufs+tpbufidx;
	return NULL;
}

static void *ec_regprint(char *loc, char *cmd, char *arg)
{
	if (*loc) {
		int reg = atoi(loc);
		if (reg < 0)
			return xserr;
		if (*arg)
			ex_regput(reg, arg, cmd[3] == '+');
		else
			xdefreg = reg;
		return NULL;
	}
	char buf[16];
	int flg = (xvis & 2) == 0;
	int wid = itoalen(xregs_n - 1);
	preserve(int, xtd, xtd = 2;)
	for (int i = 0; i < xregs_n; i++) {
		if (xregs[i] && !(xpr > 0 && i == xpr)) {
			char *e = buf;
			for (int p = itoalen(i); p < wid; p++)
				*e++ = ' ';
			e = itoa(i, e);
			*e++ = ' ';
			*e++ = i > 0 && i < 256 ? i : ' ';
			*e++ = ' ';
			*e = '\0';
			ex_cprint2(buf, msg_ft, -1, 0, 0, flg)
			ex_cprint2(xregs[i]->s, msg_ft, -1, xleft ? 0 : e - buf, xleft, !flg)
		}
	}
	restore(xtd)
	return NULL;
}

static void *ec_setenc(char *loc, char *cmd, char *arg)
{
	if (cmd[0] == 'p') {
		if (!*arg) {
			if (ph != _ph)
				free(ph);
			phlen = LEN(_ph);
			ph = _ph;
			return NULL;
		} else if (ph == _ph) {
			ph = NULL;
			phlen = 0;
		}
		ph = erealloc(ph, sizeof(struct placeholder) * (phlen + 1));
		ph[phlen].cp[0] = strtol(arg, &arg, 10);
		ph[phlen].cp[1] = strtol(arg, &arg, 10);
		ph[phlen].wid = strtol(arg, &arg, 10);
		ph[phlen].wid = MAX(0, ph[phlen].wid);
		ph[phlen].l = strtol(arg, &arg, 10);
		if (*arg == ' ')
			arg++;
		int len = strlen(arg);
		if (len && len < LEN(ph[0].d))
			memcpy(ph[phlen++].d, arg, len + 1);
		return NULL;
	}
	if (cmd[1] == 'z')
		zwlen = !zwlen ? def_zwlen : 0;
	else if (cmd[1] == 'b')
		bclen = !bclen ? def_bclen : 0;
	else if (utf8_length[0xc0] == 1) {
		memset(utf8_length+0xc0, 2, 0xe0 - 0xc0);
		memset(utf8_length+0xe0, 3, 0xf0 - 0xe0);
		memset(utf8_length+0xf0, 4, 0xf8 - 0xf0);
	} else
		memset(utf8_length+1, 1, 255);
	return NULL;
}

static void *ec_specials(char *loc, char *cmd, char *arg)
{
	static int *const sp[] = {&xesc, &xsep, &xexp, &xexe};
	int i = 0;
	if (*loc) {
		i = (unsigned char)*loc ^ '0';
		if (!*arg && i < LEN(sp))
			*sp[i] = cmd[2] ? 0 : "\\:%!"[i];
	} else
		for (int j = 0; j < LEN(sp); j++)
			*sp[j] = cmd[2] ? 0 : "\\:%!"[j];
	for (; *arg && i < LEN(sp); i++)
		*sp[i] = *arg++;
	return NULL;
}

void ex_regesc(sbuf *sb, char *beg, char *end, int ex)
{
	for (; beg < end; beg++) {
		if (*beg == '\\') {
			/* class form is safe in any layer */
			sbuf_str(sb, "[\\\\]")
			continue;
		}
		if (ex && (*beg == xsep || *beg == xexp || *beg == xexe))
			sbuf_chr(sb, xesc)
		else if (strchr("!%{[().?^$|*/+", *beg))
			sbuf_chr(sb, '\\')
		sbuf_chr(sb, *beg)
	}
}

static void *ec_krsset(char *loc, char *cmd, char *arg)
{
	if (*arg && !*loc)
		ex_krsset(arg, +1);
	else {
		int beg, end, o1 = 0, o2 = -1;
		if (ex_region(loc, &beg, &end, &o1, &o2))
			return xrerr;
		sbuf reg;
		lbuf_region(xb, &reg, beg, o1, end - 1, o2);
		sbuf_smake(sb, 64)
		ex_regesc(sb, reg.s, reg.s + reg.s_n, 1);
		free(reg.s);
		sbuf_nul(sb)
		ex_krsset(sb->s, +1);
		free(sb->s);
	}
	return xkwdrs ? NULL : xserr;
}

static int eo_val(char *arg)
{
	return uc_isdigit(*arg) || (*arg == '-' && uc_isdigit(arg[1])) ?
		atoi(arg) : (unsigned char)*arg;
}

#define _EO(opt, inner) \
static void *eo_##opt(char *loc, char *cmd, char *arg) { inner }

#define EO(opt) \
	_EO(opt, x##opt = *arg ? eo_val(arg) : !x##opt; return NULL;)

EO(pac) EO(pr) EO(ai) EO(err) EO(fr) EO(ish) EO(ic) EO(mpt)
EO(rr) EO(shape) EO(seq) EO(td) EO(order) EO(hll) EO(hlw)
EO(hlp) EO(hlr) EO(hl) EO(lim) EO(led) EO(vis)

_EO(ts, xts = *arg ? eo_val(arg) : !xts; xts = MAX(0, xts); return NULL;)
_EO(grp, xgrp = (*arg ? eo_val(arg) : !xgrp) * 2; xgrp = MAX(0, xgrp); return NULL;)

_EO(left,
	if (*loc)
		xleft = (xcols / 2) * atoi(loc);
	else if (*arg)
		xleft = atoi(arg);
	else if (lbuf_get(xb, xrow))
		xleft = ren_position(lbuf_get(xb, xrow))->pos[MIN(xoff, rstate->n)];
	return NULL;
)

#undef EO
#define EO(opt) {#opt, eo_##opt}

/* commands & opts must be sorted longest of its kind topmost */
static struct excmd {
	char *name;
	void *(*ec)(char *loc, char *cmd, char *arg);
} excmds[] = {
	{"@", ec_termexec},
	{"&", ec_termexec},
	{"!", ec_exec},
	{"=?", ec_num},
	{"=", ec_num},
	{"???", ec_while},
	{"?""?!", ec_while},
	{"??", ec_while},
	{"?!", ec_while},
	{"?", ec_while},
	{"bp", ec_setpath},
	{"bs", ec_bufsave},
	{"bx", ec_setbufsmax},
	{"b", ec_buffer},
	EO(pac),
	EO(pr),
	{"pu", ec_put},
	{"ph", ec_setenc},
	{"p", ec_print},
	EO(ai),
	{"ac", ec_setacreg},
	EO(err),
	{"ef!", ec_fuzz},
	{"ef", ec_fuzz},
	{"e!", ec_edit},
	{"e", ec_edit},
	{"ft", ec_ft},
	{"fd", ec_setdir},
	{"fp", ec_setdir},
	EO(fr),
	{"f+", ec_find},
	{"f-", ec_find},
	{"f>", ec_find},
	{"f<", ec_find},
	{"f", ec_fuzz},
	EO(ish),
	{"inc", ec_setincl},
	EO(ic),
	{"i", ec_insert},
	{"d", ec_delete},
	EO(grp),
	{"g!", ec_glob},
	{"g", ec_glob},
	EO(mpt),
	{"m", ec_mark},
	{"q!", ec_quit},
	{"q", ec_quit},
	{"reg+", ec_regprint},
	{"reg", ec_regprint},
	{"re", ec_krsset},
	{"rd", ec_undoredo},
	EO(rr),
	{"r", ec_read},
	{"wq!", ec_write},
	{"wq", ec_write},
	{"w!", ec_write},
	{"w", ec_write},
	{"uc", ec_setenc},
	{"uz", ec_setenc},
	{"ub", ec_setenc},
	{"ud", ec_undoredo},
	EO(shape),
	EO(seq),
	{"sc!", ec_specials},
	{"sc", ec_specials},
	{"s", ec_substitute},
	{"x!", ec_write},
	{"x", ec_write},
	{"ya!", ec_yank},
	{"ya+", ec_yank},
	{"ya", ec_yank},
	{"cm!", ec_cmap},
	{"cm", ec_cmap},
	{"cd", ec_chdir},
	{"c", ec_insert},
	{"j", ec_join},
	EO(ts),
	EO(td),
	EO(order),
	EO(hll),
	EO(hlw),
	EO(hlp),
	EO(hlr),
	EO(hl),
	EO(left),
	EO(lim),
	EO(led),
	EO(vis),
	{"", ec_print}, /* do not remove */
	{"", ec_print}, /* do not remove */
};

/* parse command argument expanding % and ! */
static const char *ex_arg(const char *src, sbuf *sb, int *arg)
{
	*arg = sb->s_n;
	while (*src && *src != xsep) {
		if (*src == xexp) {
			int n;
			struct buf *pbuf = ex_buf;
			src++;
			if (*src == '@') {
				src++;
				if (uc_isdigit(*src)) {
					for (n = 0; uc_isdigit(*src); src++)
						n = n * 10 + (*src - '0');
					sbuf *reg = ex_regget(n);
					if (reg)
						sbuf_mem(sb, reg->s, reg->s_n)
					pbuf = NULL;
				}
			} else if (*src == '#') {
				src++;
				pbuf = ex_pbuf;
			} else if (uc_isdigit(*src)) {
				for (n = 0; uc_isdigit(*src); src++)
					n = n * 10 + (*src - '0');
				pbuf = &bufs[n];
			}
			if (pbuf >= bufs && pbuf < &bufs[xbufcur] && pbuf->path[0])
				sbuf_mem(sb, pbuf->path, pbuf->plen)
			if (src[-1] == '@')
				sbuf_chr(sb, '@')
			src += *src == xesc && src[-1] != '#' && uc_isdigit(src[1]);
		} else if (*src == xexe) {
			int n = sb->s_n;
			src++;
			ex_sread(sb, (char**)&src, xexe, xesc);
			sbuf_cut(sb, n)
			sbuf *str = cmd_pipe(sb->s + n, NULL, 1, NULL);
			if (str) {
				sbuf_mem(sb, str->s, str->s_n)
				sbuf_free(str)
			}
		} else if (*src == xesc) {
			ex_parity(src, sb, xesc,
				src[n] == xsep || src[n] == xexp || src[n] == xexe)
		} else
			sbuf_chr(sb, *src++)
	}
	sbuf_nul(sb)
	return src;
}

/* parse prefix and command */
static const char *ex_cmd(const char *src, sbuf *sb, int *idx)
{
	int i, j;
	if ((*src && *src == xsep) || (*idx == LEN(excmds) - 1))
		src++;
	while (memchr(" \t0123456789+-.,<>/$';%*#|", *src, 26)) {
		if (*src == '>' || *src == '<' || *src == '|') {
			int esc = 0;
			j = *src;
			i = j == '|' ? xesc : '\\';
			do {
				esc = *src == i && !esc;
				sbuf_chr(sb, *src++)
			} while (*src && (*src != j || esc));
			if (!*src)
				break;
		} else if (*src == ' ' || *src == '\t') {
			src++;
			continue;
		}
		sbuf_chr(sb, *src++)
	}
	sbuf_chr(sb, '\0')
	if (*src == xsep) {
		*idx = LEN(excmds) - 1;
		return src;
	}
	for (i = 0; i < LEN(excmds); i++) {
		for (j = 0; excmds[i].name[j]; j++)
			if (!src[j] || src[j] != excmds[i].name[j])
				break;
		if (!excmds[i].name[j]) {
			*idx = i;
			src += j;
			break;
		}
	}
	if (*src == ' ' || *src == '\t')
		src++;
	return src;
}

/* execute a single ex command chain */
void *ex_exec(const char *ln)
{
	int arg, idx = 0;
	char *ret = NULL;
	preserve(int, xquit, xquit = 0;)
	if (!xexec_dep)
		lbuf_mark(xb, '*', xrow, xoff);
	xexec_dep++;
	sbuf_smake(sb, 128)
	do {
		sbuf_cut(sb, 0)
		ln = ex_arg(ex_cmd(ln, sb, &idx), sb, &arg);
		ret = excmds[idx].ec(sb->s, excmds[idx].name, sb->s + arg);
		xpret = ret;
		if (ret && ret != xuerr && xerr & 1) {
			ex_print(ret, msg_ft)
			ret = xuerr;
		}
		if (ret && xerr & 2)
			break;
	} while (*ln && !xquit);
	free(sb->s);
	xexec_dep--;
	if ((xquit > 0 && (xexec_dep || xqprop >= 0) && --xqprop < 0)
			|| tmpxquit < -256)
		restore(xquit)
	if (!xexec_dep) {
		if (xanchor) {
			sbuf_free(xanchor)
			xanchor = NULL;
		}
		xqprop = 0;
	}
	return xerr & 4 ? NULL : ret;
}

/* ex main loop */
void ex(void)
{
	xgrec++;
	int esc = 0;
	sbuf_smake(sb, xcols)
	while (!xquit) {
		syn_setft(ex_ft);
		if (ex_read(sb, ":", NULL, 0, 1) == '\n') {
			if (!strcmp(sb->s, ":") && esc) {
				xpln = 2;
				ex_exec(ex_regget(':')->s);
			} else
				ex_command(sb->s + !(xvis & 1))
			xb->useq += xseq;
			esc = 1;
		} else
			esc = 0;
		sbuf_cut(sb, 0)
	}
	syn_setft(xb_ft);
	free(sb->s);
	xgrec--;
}

void ex_init(char **files, int n)
{
	xbufsalloc = MAX(n, xbufsalloc);
	ec_setbufsmax(NULL, NULL, "");
	char *s = files[0] ? files[0] : "";
	do {
		xmpt = 0;
		ec_edit("", "e", s);
		s = *(++files);
	} while (--n > 0);
	xvis &= ~4;
	if ((s = getenv("EXINIT")))
		ex_command(s)
}
