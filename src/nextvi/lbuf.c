struct lbuf *lbuf_make(void)
{
	struct lbuf *lb = emalloc(sizeof(*lb));
	memset(lb, 0, sizeof(*lb));
	lb->mark_sb[0] = -1;
	lb->mark_se[0] = -1;
	return lb;
}

static void lopt_done(struct lopt *lo)
{
	free(lo->mark);
	if (!(lo->ref & 2))
		for (int i = 0; i < lo->n_ins; i++)
			free(lbuf_s(lo->ins[i]));
	free(lo->ins);
	if (!(lo->ref & 1))
		for (int i = 0; i < lo->n_del; i++)
			free(lbuf_s(lo->del[i]));
	free(lo->del);
}

#define lbuf_copymark(dst, src) { dst[0] = src[0]; dst[1] = src[1]; }

/* find a mark id, returning its row & off pair */
static int *mark_find(int *mark, int n, int id)
{
	for (int i = 0; i < n * 3; i += 3)
		if (mark[i] == id)
			return mark + i + 1;
	return NULL;
}

static void mark_set(int **mark, int *n, int id, int pos, int off)
{
	int *m = mark_find(*mark, *n, id);
	if (!m) {
		*mark = erealloc(*mark, (*n + 1) * 3 * sizeof(int));
		m = *mark + *n * 3;
		*m++ = id;
		(*n)++;
	}
	m[0] = pos;
	m[1] = off;
}

void lbuf_mark(struct lbuf *lb, int mk, int pos, int off)
{
	if (mk == '\'')
		mk = '`';
	if (mk == '[') {
		lb->mark_sb[0] = pos;
		lb->mark_sb[1] = off;
	} else if (mk == ']') {
		lb->mark_se[0] = pos;
		lb->mark_se[1] = off;
	} else
		mark_set(&lb->mark, &lb->mark_n, mk, pos, off);
}

int lbuf_jump(struct lbuf *lb, int mk, int *pos, int *off)
{
	int *m;
	if (mk == '\'')
		mk = '`';
	if (mk == '[')
		m = lb->mark_sb;
	else if (mk == ']')
		m = lb->mark_se;
	else
		m = mark_find(lb->mark, lb->mark_n, mk);
	if (!m || m[0] < 0)
		return 1;
	*pos = m[0];
	*off = MAX(0, m[1]);
	return 0;
}

void lbuf_free(struct lbuf *lb)
{
	int i;
	for (i = 0; i < lb->ln_n; i++)
		free(lbuf_i(lb, i));
	for (i = 0; i < lb->hist_n; i++)
		lopt_done(&lb->hist[i]);
	free(lb->hist);
	free(lb->mark);
	free(lb->ln);
	free(lb);
}

static int linelength(char *s)
{
	int len = dstrlen(s, '\n');
	return s[len] == '\n' ? len + 1 : len;
}

/* low-level line replacement */
static int lbuf_replace(struct lbuf *lb, sbuf *sb, char *s, struct lopt *lo, int n_del, int n_ins)
{
	int i, pos = lo->pos;
	if (s) {
		for (; *s; n_ins++) {
			int l = linelength(s);
			int l_nonl = l - (s[l - !!l] == '\n');
			struct linfo *n = emalloc(l_nonl + 5 + sizeof(struct linfo));
			n->len = l_nonl;
			n->grec = 0;
			char *ln = (char*)(n + 1);
			memcpy(ln, s, l_nonl);
			memset(&ln[l_nonl + 1], 0, 4);	/* fault tolerance pad */
			ln[l_nonl] = '\n';
			sbuf_mem(sb, &ln, sizeof(s))
			s += l;
		}
	}
	if (lb->ln_n + n_ins - n_del >= lb->ln_sz) {
		int nsz = lb->ln_n + n_ins - n_del + 512;
		char **nln = emalloc(nsz * sizeof(lb->ln[0]));
		memcpy(nln, lb->ln, lb->ln_n * sizeof(lb->ln[0]));
		free(lb->ln);
		lb->ln = nln;
		lb->ln_sz = nsz;
	}
	if (n_ins != n_del) {
		memmove(lb->ln + pos + n_ins, lb->ln + pos + n_del,
			(lb->ln_n - pos - n_del) * sizeof(lb->ln[0]));
	}
	lb->ln_n += n_ins - n_del;
	for (i = 0; i < n_ins; i++)
		lb->ln[pos + i] = *((char**)sb->s + i);
	for (i = 0; i < lb->mark_n; i++) {	/* updating marks */
		int *m = lb->mark + i * 3, *lm;
		if (m[1] >= pos + n_ins && m[1] < pos + n_del) {
			mark_set(&lo->mark, &lo->mark_n, m[0], m[1], m[2]);
			m[1] = n_ins ? pos + n_ins - 1 : -1;
		} else if (m[1] >= pos + n_del) {
			m[1] += n_ins - n_del;
		} else if ((lm = mark_find(lo->mark, lo->mark_n, m[0])))
			lbuf_copymark((m + 1), lm)
	}
	return n_ins;
}

void lbuf_smark(struct lbuf *lb, struct lopt *lo, int beg, int o1)
{
	lbuf_copymark(lo->mark_sb, lb->mark_sb)
	lb->mark_sb[0] = beg;
	lb->mark_sb[1] = o1;
}

void lbuf_emark(struct lbuf *lb, struct lopt *lo, int end, int o2)
{
	lbuf_copymark(lo->mark_se, lb->mark_se)
	lb->mark_se[0] = end;
	lb->mark_se[1] = o2;
	if (xseq < 0)
		lopt_done(lo);
}

/* append undo/redo history */
struct lopt *lbuf_opt(struct lbuf *lb, int beg, int o1, int n_del)
{
	struct lopt *lo;
	static struct lopt slo;
	if (xseq < 0)
		lo = &slo;
	else {
		for (int i = lb->hist_u; i < lb->hist_n; i++)
			lopt_done(&lb->hist[i]);
		lb->hist_n = lb->hist_u;
		if (lb->hist_n == lb->hist_sz) {
			int sz = lb->hist_sz + (lb->hist_sz ? lb->hist_sz : 128);
			struct lopt *hist = emalloc(sz * sizeof(hist[0]));
			memcpy(hist, lb->hist, lb->hist_n * sizeof(hist[0]));
			free(lb->hist);
			lb->hist = hist;
			lb->hist_sz = sz;
		}
		lo = &lb->hist[lb->hist_n++];
		lb->hist_u = lb->hist_n;
	}
	lo->ins = NULL;
	lo->del = n_del ? emalloc(n_del * sizeof(lo->del[0])) : NULL;
	for (int i = 0; i < n_del; i++)
		lo->del[i] = lb->ln[beg + i];
	lo->mark = NULL;
	lo->mark_n = 0;
	lo->mark_sb[0] = -1;
	lo->mark_se[0] = -1;
	lo->pos = beg;
	lo->pos_off = o1;
	lo->n_ins = 0;
	lo->n_del = n_del;
	lo->seq = lb->useq;
	lo->ref = 2;
	return lo;
}

/* replace lines beg through end with buf */
void lbuf_edit(struct lbuf *lb, char *buf, int beg, int end, int o1, int o2)
{
	if (beg > lb->ln_n)
		beg = lb->ln_n;
	if (end > lb->ln_n)
		end = lb->ln_n;
	if (beg == end && !buf)
		return;
	struct lopt *lo = lbuf_opt(lb, beg, o1, end - beg);
	sbuf_smake(sb, sizeof(lo->ins[0])+1)
	lo->n_ins = lbuf_replace(lb, sb, buf, lo, lo->n_del, 0);
	if (lb->hist_u < 2 || lb->hist[lb->hist_u - 2].seq != lb->useq)
		lbuf_smark(lb, lo, beg, o1);
	lbuf_emark(lb, lo, beg + (lo->n_ins ? lo->n_ins - 1 : 0), o2);
	lb->modified = 1;
	if (lb->saved > lb->hist_u)
		lb->saved = -1;
	if (xseq < 0 || !lo->n_ins)
		free(sb->s);
	else
		lo->ins = (char**)sb->s;
}

int lbuf_rd(struct lbuf *lb, int fd, int beg, int end)
{
	struct stat st;
	long nr;	/* 1048575 caps at 2147481600 on 32 bit */
	int sz = 1048575, step = 1, n = 0;
	if (fstat(fd, &st) >= 0 && S_ISREG(st.st_mode))
		sz = st.st_size >= INT_MAX ? INT_MAX : st.st_size + step;
	char *s = emalloc(sz--);
	while ((nr = read(fd, s + n, sz - n)) > 0) {
		n += nr;
		if (n >= sz + step) {
			if (n > INT_MAX / 2) {
				n -= nr;
				break;
			}
			sz = n * 2;
			s = erealloc(s, sz--);
			step = 1;
		} else if (n == sz) {
			sz++;
			step = 0;
		}
	}
	s[n] = '\0';
	lbuf_edit(lb, s, beg, end, 0, 0);
	free(s);
	return nr != 0;
}

int lbuf_wr(struct lbuf *lb, int fd, int beg, int end)
{
	for (int i = beg; i < end; i++) {
		char *ln = lb->ln[i];
		long nw = 0;
		long nl = lbuf_s(ln)->len + 1;
		while (nw < nl) {
			long nc = write(fd, ln + nw, nl - nw);
			if (nc < 0)
				return nc;
			nw += nc;
		}
	}
	return 0;
}

void lbuf_region(struct lbuf *lb, sbuf *sb, int r1, int o1, int r2, int o2)
{
	char *s1 = lbuf_get(lb, r1), *s2;
	_sbuf_make(sb, 1024,)
	r2 = MIN(lb->ln_n, r2);
	if (s1) {
		char *send = s1 + lbuf_s(s1)->len+1;
		if (r1 == r2) {
			s1 = uc_chr(s1, o1);
			s2 = o2 >= o1 ? uc_chr(s1, o2 - o1) : send;
			if (s2 > s1)
				sbuf_mem(sb, s1, s2 - s1)
			goto ret;
		}
		s2 = o1 >= 0 ? uc_chr(s1, o1) : send;
		if (send > s2)
			sbuf_mem(sb, s2, send - s2)
	}
	for (int i = r1 + 1; i < r2; i++)
		sbuf_mem(sb, lb->ln[i], lbuf_i(lb, i)->len + 1)
	if ((s2 = lbuf_get(lb, r2))) {
		s1 = o2 >= 0 ? uc_chr(s2, o2) : s2 + lbuf_s(s2)->len+1;
		if (s1 > s2)
			sbuf_mem(sb, s2, s1 - s2)
	}
	ret:
	sbuf_nul4(sb)
}

/* convert (row, off) position to byte offset within region (r1,o1)-(r2,o2) */
int lbuf_pos2off(struct lbuf *lb, int r1, int o1, int r2, int o2, int row, int off)
{
	int boff = 0, sub;
	char *ln = lbuf_get(lb, r1);
	if (!ln || row < r1 || row > r2)
		return -1;
	for (int i = r1; ln && i <= row;) {
		if (i == row) {
			if ((i == r1 && off < o1) || (o2 >= 0 && i == r2 && off > o2))
				return -1;
			sub = uc_chr(lb->ln[r1], o1) - lb->ln[r1];
			boff -= boff ? sub : 0;
			if (i != r1)
				sub = 0;
			return boff + (uc_chr(ln, off) - (ln + sub));
		}
		boff += lbuf_i(lb, i)->len + 1;
		ln = lbuf_get(lb, ++i);
	}
	return -1;
}

/* convert byte offset within region (r1,o1)-(r2,o2) to (row, off) position */
int lbuf_off2pos(struct lbuf *lb, int r1, int o1, int r2, int o2, int boff, int *row, int *off)
{
	char *ln = lbuf_get(lb, r1);
	if (!ln)
		return 1;
	int acc = -(uc_chr(ln, o1) - ln);
	for (int i = r1; ln && i <= r2; ln = lbuf_get(lb, ++i)) {
		acc += lbuf_i(lb, i)->len + 1;
		if (acc > boff) {
			*row = i;
			*off = uc_off(ln, boff - (acc - (lbuf_i(lb, i)->len + 1)));
			return o2 >= 0 && i == r2 && *off > o2;
		}
	}
	return 1;
}

char *lbuf_joinsb(struct lbuf *lb, int r1, int r2, sbuf *i, int *o1, int *o2)
{
	char *s = lbuf_get(lb, r1), *e, *se, *p;
	char *es = lbuf_get(lb, r2);
	int endsz;
	if (!s || !es)
		return NULL;
	if (rstate->s == s) {
		*o1 = MIN(*o1, rstate->n);
		e = rstate->chrs[*o1];
	} else
		e = uc_chrn(s, *o1, o1);
	if (r1 == r2) {
		se = *o2 > *o1 ? uc_chr(e, *o2 - *o1) :
			*o2 == -2 ? s + lbuf_s(s)->len + 1 : e;	/* -2 eol, -1 point default */
		endsz = lbuf_s(s)->len + 5 - (se - e);
	} else {
		se = uc_chrn(es, *o2, o2);
		endsz = (e - s) + (lbuf_s(es)->len + 5 - (se - es));
	}
	p = emalloc(endsz + i->s_n);
	memcpy(p, s, e - s);
	memcpy(p + (e - s), i->s, i->s_n);
	memcpy(p + (e - s) + i->s_n, se, endsz - (e - s));
	return p;
}

int lbuf_join(struct lbuf *lb, int beg, int end, int o1, int *o2, int flg)
{
	if (!lbuf_get(lb, beg) || !lbuf_get(lb, end - 1))
		return 1;
	sbuf_smake(sb, 1024)
	for (int i = beg; i < end; i++) {
		char *ln = lbuf_get(lb, i);
		char *lnend = ln + lbuf_s(ln)->len;
		if (i > beg) {
			while (ln[0] == ' ' || ln[0] == '\t')
				ln++;
			if (flg && sb->s_n && *ln != ')' &&
					sb->s[sb->s_n-1] != ' ') {
				sbuf_chr(sb, ' ')
				*o2 += 1;
			}
		}
		*o2 += (i+1 == end) ? 0 : uc_slen(ln) - 1;
		sbuf_mem(sb, ln, lnend - ln)
	}
	sbufn_chr(sb, '\n')
	lbuf_edit(lb, sb->s, beg, end, o1, *o2);
	free(sb->s);
	return 0;
}

char *lbuf_get(struct lbuf *lb, int pos)
{
	return pos >= 0 && pos < lb->ln_n ? lb->ln[pos] : NULL;
}

int lbuf_undo(struct lbuf *lb, int *row, int *off)
{
	if (!lb->hist_u)
		return 1;
	struct lopt *lo = &lb->hist[lb->hist_u - 1];
	const int useq = lo->seq;
	sbuf sb;
	if (lb->hist_u == lb->hist_n) {
		lbuf_copymark(lb->tmp_mark, lb->mark_sb)
		lbuf_copymark((lb->tmp_mark + 2), lb->mark_se)
	}
	while (lb->hist_u && lb->hist[lb->hist_u - 1].seq == useq) {
		lo = &lb->hist[--lb->hist_u];
		lo->ref = 1;
		sb.s = (char*)lo->del;
		lbuf_replace(lb, &sb, NULL, lo, lo->n_ins, lo->n_del);
	}
	*row = lo->pos;
	*off = MAX(0, lo->pos_off);
	lbuf_copymark(lb->mark_sb, lo->mark_sb)
	lbuf_copymark(lb->mark_se, lo->mark_se)
	lb->modified = lb->hist_u != lb->saved;
	return 0;
}

int lbuf_redo(struct lbuf *lb, int *row, int *off)
{
	if (lb->hist_u == lb->hist_n)
		return 1;
	struct lopt *lo = &lb->hist[lb->hist_u];
	const int useq = lo->seq;
	sbuf sb;
	while (lb->hist_u < lb->hist_n && lb->hist[lb->hist_u].seq == useq) {
		lo = &lb->hist[lb->hist_u++];
		lo->ref = 2;
		sb.s = (char*)lo->ins;
		lbuf_replace(lb, &sb, NULL, lo, lo->n_del, lo->n_ins);
	}
	*row = lo->pos;
	*off = MAX(0, lo->pos_off);
	if (lb->hist_u < lb->hist_n) {
		lo++;
		lbuf_copymark(lb->mark_sb, lo->mark_sb)
		lbuf_copymark(lb->mark_se, lo->mark_se)
	} else {
		lbuf_copymark(lb->mark_sb, lb->tmp_mark)
		lbuf_copymark(lb->mark_se, (lb->tmp_mark + 2))
	}
	lb->modified = lb->hist_u != lb->saved;
	return 0;
}

/* mark buffer as saved and, if clear, clear the undo history */
void lbuf_saved(struct lbuf *lb, int clear)
{
	if (clear) {
		for (int i = 0; i < lb->hist_n; i++)
			lopt_done(&lb->hist[i]);
		lb->hist_n = 0;
		lb->hist_u = 0;
	}
	lb->modified = 0;
	lb->saved = lb->hist_u;
}

int lbuf_indents(struct lbuf *lb, int r)
{
	char *ln = lbuf_get(lb, r);
	int o;
	if (!ln)
		return 0;
	for (o = 0; uc_isspace(*ln); o++)
		ln += uc_len(ln);
	return *ln ? o : o - 2;
}

int lbuf_findchar(struct lbuf *lb, char *cs, int cmd, int n, int *row, int *off)
{
	char *ln = lbuf_get(lb, *row);
	int c1, c2, l, dir = (cmd == 'f' || cmd == 't') ? +1 : -1;
	if (!ln)
		return 1;
	ren_state *r = ren_position(ln);
	uc_code(c2, cs, l)
	*off += dir + (cmd == 't') - (cmd == 'T');
	while (n > 0 && *off >= 0 && *off < r->n) {
		uc_code(c1, r->chrs[*off], l)
		if (c1 == c2)
			n--;
		if (n > 0)
			*off += dir;
	}
	if (!n && (cmd == 't' || cmd == 'T'))
		*off = MIN(MAX(0, *off - dir), r->n-1);
	return n != 0;
}

int lbuf_search(struct lbuf *lb, rset *re, int dir, int beg, int end, int pskip,
		int nskip, int *r, int *o)
{
	int r0 = *r, o0 = *o;
	int offs[re->nsubc], i = r0;
	char *s = lbuf_get(lb, i);
	int off, g1, g2, _o, step, flg;
	if (pskip >= 0 && s)
		off = rstate->s == s ? rstate->chrs[MIN(o0 + pskip, rstate->n)] - s
					: uc_chr(s, o0 + pskip) - s;
	else
		off = 0;
	for (; i >= beg && i < end; i += dir) {
		_o = 0;
		step = 0;
		flg = REG_NEWLINE;
		s = lb->ln[i];
		while (rset_find(re, s + off, offs, flg) >= 0) {
			flg |= REG_NOTBOL;
			g1 = offs[xgrp], g2 = offs[xgrp + 1];
			if (g1 < 0) {
				off += offs[1] > 0 ? offs[1] : 1;
				continue;
			}
			_o += uc_off(s + step, off + g1 - step);
			if (dir < 0 && r0 == i && _o > o0 - nskip)
				break;
			*o = _o;
			*r = i;
			if (dir > 0)
				return 0;
			step = off + g1;
			off += g2 > 0 ? g2 : 1;
			end = -1; /* break outer loop efficiently */
		}
		off = 0;
	}
	return end < 0 ? 0 : 1;
}

int lbuf_sectionbeg(struct lbuf *lb, int dir, int *row, int *off, int ch)
{
	if (ch == '\n')
		while (*row >= 0 && *row < lbuf_len(lb) && *lbuf_get(lb, *row) == ch)
			*row += dir;
	else
		*row += dir;
	while (*row >= 0 && *row < lbuf_len(lb) && *lbuf_get(lb, *row) != ch)
		*row += dir;
	*row = MAX(0, MIN(*row, lbuf_len(lb) - 1));
	*off = 0;
	return 0;
}

int lbuf_eol(struct lbuf *lb, int row, int state)
{
	char *ln = lbuf_get(lb, row);
	if (!ln)
		return 0;
	if (state == 2)
		state = rstate->s == ln;
	state = state ? ren_position(ln)->n - 1 : uc_slen(ln) - 1;
	return state < 0 ? 0 : state;
}

int lbuf_next(struct lbuf *lb, int dir, int *r, int *o)
{
	int odir = dir > 0 ? 1 : -1;
	int len, off = *o + odir;
	if (lbuf_get(lb, *r))
		len = ren_position(lbuf_get(lb, *r))->n;
	else
		return -1;
	if (off < 0 || off >= len) {
		if (dir % 2 == 0 || !lbuf_get(lb, *r + odir))
			return -1;
		*r += odir;
		if (odir > 0) {
			ren_position(lbuf_get(lb, *r));
			*o = 0;
		} else
			*o = lbuf_eol(lb, *r, 1);
	} else
		*o = off;
	return 0;
}

/* move to the last character of the word */
static int lbuf_wordlast(struct lbuf *lb, int kind, int dir, int *row, int *off)
{
	if (!kind || !(uc_kind(rstate->chrs[*off]) & kind))
		return 0;
	while (uc_kind(rstate->chrs[*off]) & kind)
		if (lbuf_next(lb, dir, row, off))
			return 1;
	if (!(uc_kind(rstate->chrs[*off]) & kind))
		lbuf_next(lb, -dir, row, off);
	return 0;
}

int lbuf_wordbeg(struct lbuf *lb, int big, int dir, int *row, int *off)
{
	int nl;
	if (!lbuf_get(lb, *row))
		return 1;
	ren_state *r = ren_position(lbuf_get(lb, *row));
	lbuf_wordlast(lb, big ? 3 : uc_kind(r->chrs[*off]), dir, row, off);
	nl = *rstate->chrs[*off] == '\n';
	if (lbuf_next(lb, dir, row, off))
		return 1;
	while (uc_isspace(*rstate->chrs[*off])) {
		nl += *rstate->chrs[*off] == '\n';
		if (nl == 2)
			return 0;
		if (lbuf_next(lb, dir, row, off))
			return 1;
	}
	return 0;
}

int lbuf_wordend(struct lbuf *lb, int big, int dir, int *row, int *off)
{
	int nl = 0;
	if (!lbuf_get(lb, *row))
		return 1;
	ren_state *r = ren_position(lbuf_get(lb, *row));
	if (!uc_isspace(*r->chrs[*off])) {
		if (lbuf_next(lb, dir, row, off))
			return 1;
		nl = dir < 0 && *rstate->chrs[*off] == '\n';
	}
	nl += dir > 0 && *rstate->chrs[*off] == '\n';
	while (uc_isspace(*rstate->chrs[*off])) {
		if (lbuf_next(lb, dir, row, off))
			return 1;
		nl += *rstate->chrs[*off] == '\n';
		if (nl == 2) {
			if (dir < 0)
				lbuf_next(lb, -dir, row, off);
			return 0;
		}
	}
	lbuf_wordlast(lb, big ? 3 : uc_kind(rstate->chrs[*off]), dir, row, off);
	return 0;
}

/* move to the matching character */
int lbuf_pair(struct lbuf *lb, char *pairs, int plen, int *row, int *off)
{
	int r = *row, o = *off;
	int p, c, dep = 1;
	char *m;
	if (!lbuf_get(lb, r))
		return 1;
	ren_state *rs = ren_position(lbuf_get(lb, r));
	for (; o < rs->n-1 && !memchr(pairs, *rs->chrs[o], plen); o++);
	if (!(m = memchr(pairs, *rs->chrs[o], plen)))
		return 1;
	p = m - pairs;
	while (!lbuf_next(lb, (p & 1) ? -1 : +1, &r, &o)) {
		c = *rstate->chrs[o];
		if (c == pairs[p ^ 1])
			dep--;
		if (c == pairs[p])
			dep++;
		if (!dep) {
			*row = r;
			*off = o;
			return 0;
		}
	}
	return 1;
}
