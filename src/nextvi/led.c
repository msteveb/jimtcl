static sbuf *suggestsb;
static sbuf *acsb;
sbuf *led_attsb;

int dstrlen(const char *s, char delim)
{
	register const char *i;
	for (i=s; *i && *i != delim; ++i);
	return i-s;
}

static int search(const char *pattern, int l)
{
	if (!*pattern)
		return 0;
	sbuf_cut(suggestsb, 0)
	sbuf_smake(sylsb, 1024)
	char *part = strstr(acsb->s, pattern);
	while (part) {
		char *part1 = part;
		while (*part != '\n')
			part--;
		int len = dstrlen(++part, '\n');
		if (len++ != l) {
			if (part == part1)
				sbuf_mem(suggestsb, part, len)
			else
				sbuf_mem(sylsb, part, len)
		}
		part = strstr(part+len, pattern);
	}
	sbuf_mem(suggestsb, sylsb->s, sylsb->s_n)
	free(sylsb->s);
	sbuf_nul4(suggestsb)
	return suggestsb->s_n;
}

static void file_index(struct lbuf *buf)
{
	char reg[] = "[^\t !-/:-@[-\\]^`{-\x7f]+";
	int len, sidx, grp = xgrp;
	char **ss = buf->ln;
	int ln_n = lbuf_len(buf), n;
	rset *rs = rset_smake(xacreg ? xacreg->s : reg,
		xic ? REG_ICASE | REG_NEWLINE : REG_NEWLINE);
	if (!rs || grp >= rs->nsubc)
		return;
	int subs[rs->nsubc];
	sbuf_smake(ibuf, 1024)
	for (n = 1; n <= acsb->s_n; n++)
		if (acsb->s[n - 1] == '\n')
			sbuf_mem(ibuf, &n, sizeof(n))
	for (int i = 0; i < ln_n; i++) {
		sidx = 0;
		while (rset_find(rs, ss[i]+sidx, subs, sidx ? REG_NOTBOL : 0) >= 0) {
			/* if target group not found, continue with group 1
			which will always be valid, otherwise there be no match */
			if (subs[grp] < 0) {
				sidx += subs[1] > 0 ? subs[1] : 1;
				continue;
			}
			len = subs[grp + 1] - subs[grp];
			if (len > 1) {
				char *part = ss[i]+sidx+subs[grp];
				int *ip = (int*)(ibuf->s+sizeof(n));
				for (n = len+1; ip < (int*)&ibuf->s[ibuf->s_n]; ip++)
					if (*ip - ip[-1] == n &&
						!memcmp(acsb->s + ip[-1], part, len))
							goto skip;
				sbuf_mem(acsb, part, len)
				sbuf_chr(acsb, '\n')
				sbuf_mem(ibuf, &acsb->s_n, sizeof(n))
			}
			skip:
			sidx += subs[grp + 1] > 0 ? subs[grp + 1] : 1;
		}
	}
	sbuf_nul(acsb)
	free(ibuf->s);
	rset_free(rs);
}

static char *kmap_map(int kmap, int c)
{
	static char cs[4];
	char **keymap = conf_kmap(kmap);
	cs[0] = c;
	return keymap[c] ? keymap[c] : cs;
}

/* map cursor horizontal position to terminal column number */
int led_pos(char *s, int pos)
{
	if (dir_context(s) < 0)
		return xleft + xcols - pos - 1;
	return pos - xleft;
}

#define print_ch1(out) sbuf_mem(out, chrs[o], l)
#define print_ch2(out) sbuf_mem(out, *chrs[o] == ' ' ? "_" : chrs[o], l)

#define hid_ch1(out) sbuf_set(out, ' ', i - l)
#define hid_ch2(out) \
sbuf_set(out, *chrs[o] == '\n' ? '\\' : '-', i - l) \
if (ctx > 0 && *chrs[o] == '\t') \
	out->s[out->s_n-1] = '>'; \
else if (*chrs[o] == '\t') \
	out->s[out->s_n - (i - l)] = '<'; \

#define led_out(out, n) \
{ \
for (i = 0; i < cterm;) { \
	int att_new = 0; \
	o = off[i]; \
	if (o >= 0) { \
		for (l = i; off[i] == o; i++); \
		att_new = att[bound ? ctt[atti++] : o]; \
		if (att_new != att_old) \
			sbuf_str(out, term_att(att_new)) \
		char *s = ren_translate(chrs[o], s0); \
		if (s) \
			sbuf_str(out, s) \
		else if (uc_isprint(*chrs[o])) { \
			l = uc_len(chrs[o]); \
			print_ch##n(out) \
		} else { \
			hid_ch##n(out) \
		} \
	} else { \
		if (cbeg || ctx < 0) { \
			if (att_new != att_old) \
				sbuf_mem(out, "\x1b[m", 3) \
			sbuf_chr(out, ' ') \
			i++; \
		} else \
			break; \
	} \
	att_old = att_new; \
} } \

/* render and highlight a line */
void led_render(char *s0, int cbeg, int cend)
{
	if (!xled)
		return;
	ren_state *r = ren_position(s0);
	int j, c, l, i, o, n = r->n;
	int att_old = 0, atti = 0, cterm = cend - cbeg;
	char *bound = NULL;
	char **chrs = r->chrs;	/* chrs[i]: the i-th character in s0 */
	int off[cterm+1];	/* off[i]: the character at screen position i */
	int att[cterm+1];	/* att[i]: the attributes of i-th character */
	int stt[cterm+1];	/* stt[i]: remap off indexes */
	int ctt[cterm+1];	/* ctt[i]: cterm bound attrs */
	int ctx = r->ctx;
	off[cterm] = -1;
	if (ctx < 0) {
		o = cbeg;
		for (c = cterm-1; c >= 0; c--, o++)
			off[c] = o <= r->cmax ? r->col[o] : -1;
	} else {
		for (c = cbeg; c < cend; c++)
			off[c - cbeg] = c <= r->cmax ? r->col[c] : -1;
	}
	if (r->cmax > cterm || cbeg) {
		i = ctx < 0 ? cterm-1 : 0;
		o = off[i];
		if (o >= 0 && cbeg && r->pos[o] < cbeg)
			while (off[i] == o)
				off[ctx < 0 ? i-- : i++] = -1;
		i = ctx < 0 ? 0 : cterm-1;
		o = off[i];
		if (o >= 0 && r->cmax > cterm && r->pos[o] + r->wid[o] > cend)
			while (off[i] == o)
				off[ctx < 0 ? i++ : i--] = -1;
		for (i = 0, c = 0; i < cterm;) {
			if ((o = off[i++]) >= 0) {
				att[c++] = o;
				for (; off[i] == o; i++);
			}
		}
		stt[0] = 0;
		for (i = 1; i < c; i++) {
			int key0 = att[i];
			j = i - 1;
			while (j >= 0 && att[j] > key0) {
				att[j + 1] = att[j];
				stt[j + 1] = stt[j];
				j = j - 1;
			}
			att[j + 1] = key0;
			stt[j + 1] = i;
		}
		sbuf_smake(bsb, cterm*4);
		for (i = 0; i < c; i++) {
			ctt[stt[i]] = i;
			stt[i] = att[i];
			sbuf_mem(bsb, chrs[att[i]], uc_len(chrs[att[i]]))
		}
		sbuf_nul4(bsb)
		bound = bsb->s;
	}
	memset(att, 0, MIN(n, cterm+1) * sizeof(att[0]));
	if (xhl)
		syn_highlight(att, bound ? bound : s0, MIN(n, cterm));
	free(bound);
	if (led_attsb && xhl) {
		led_att *p = (led_att*)led_attsb->s;
		for (; (char*)p < &led_attsb->s[led_attsb->s_n]; p++) {
			if (p->s != s0 && p->s)
				continue;
			if (!bound)
				att[p->off] = syn_merge(att[p->off], p->att);
			else if (c && stt[0] <= p->off && stt[c-1] >= p->off) {
				i = p->off - stt[0];
				if (i < c && stt[i] == p->off) {
					att[i] = syn_merge(att[i], p->att);
					continue; /* text not reordered */
				}
				for (l = 0, j = c - 1; l <= j;) {
					i = l + (j - l) / 2;
					if (stt[i] == p->off) {
						att[i] = syn_merge(att[i], p->att);
						break;
					} else if (stt[i] < p->off)
						l = i + 1;
					else
						j = i - 1;
				}
			}
		}
	}
	if (xhlr && xhl) {
		for (l = 0, i = 0; i < cterm;) {
			o = off[i++];
			if (o < 0)
				continue;
			for (l++; off[i] == o; i++);
			if (o+1 >= n || r->pos[o] + r->wid[o] == r->pos[o + 1])
				continue;
			if (r->pos[o + 1] + r->wid[o + 1] != r->pos[o])
				continue;
			j = bound ? ctt[l-1] : o;
			att[j] = syn_merge(att[j], conf_hlrev);
			att[j+1] = syn_merge(att[j+1], conf_hlrev);
		}
	}
	/* generate term output */
	if (vi_hidch)
		led_out(term_sbuf, 2)
	else
		led_out(term_sbuf, 1)
	sbufn_mem(term_sbuf, "\x1b[m", 3)
	if (r->holelen) {
		memcpy(chrs[n], r->nulhole, r->holelen);
		r->holelen = 0;
	}
}

static int led_lastchar(char *s)
{
	char *r = *s ? strchr(s, '\0') : s;
	if (r != s)
		r = uc_beg(s, r - 1);
	return r - s;
}

static int led_lastword(char *s)
{
	char *r = *s ? uc_beg(s, strchr(s, '\0') - 1) : s;
	int kind;
	while (r > s && uc_isspace(*r))
		r = uc_beg(s, r - 1);
	kind = r > s ? uc_kind(r) : 0;
	while (r > s && uc_kind(uc_beg(s, r - 1)) == kind)
		r = uc_beg(s, r - 1);
	return r - s;
}

static void led_printparts(sbuf *sb, int pre, int ps,
	char *post, int postn, int *poff)
{
	if (!xled) {
		sbuf_nul4(sb)
		return;
	}
	int dir, off, pos, psn = sb->s_n;
	sbuf_str(sb, post)
	sbuf_nul4(sb)
	/* XXX: O(n) insertion; recursive array data structure cannot be optimized.
	For correctness, rstate must be recomputed. */
	rstate += 2;
	rstate->s = NULL;
	ren_state *r = ren_position(sb->s + ps);
	off = r->n - postn;
	*poff = off;
	pos = ren_cursor(r->s, r->pos[MAX(0, off-1)]);
	if (off > 0) {
		int two = off > 1 && psn != pre;
		dir = r->pos[off-two] - r->pos[off-(two+1)];
		if (abs(dir) > r->wid[off-(two+1)])
			pos = ren_cursor(r->s, r->pos[off-two]);
		pos += dir < 0 ? -1 : 1;
	}
	if (pos >= xleft + xcols || pos < xleft)
		xleft = pos < xcols ? 0 : pos - xcols / 2;
	syn_scdir(0);
	led_crender(r->s, -1, vi_lncol, xleft, xleft + xcols - vi_lncol);
	term_pos(-1, led_pos(r->s, pos) + vi_lncol);
	sbufn_cut(sb, psn)
	rstate -= 2;
}

/* read a character from the terminal */
char *led_read(int *kmap, int c)
{
	static char buf[5];
	int c1, c2, i, n;
	while (!TK_INT(c)) {
		switch (c) {
		case TK_CTL('f'):
			*kmap = xkmap_alt;
			break;
		case TK_CTL('e'):
			*kmap = 0;
			break;
		case TK_CTL('v'):	/* literal character */
			buf[0] = term_read(0);
			buf[1] = '\0';
			return buf;
		case TK_CTL('k'):	/* digraph */
			c1 = term_read(0);
			if (TK_INT(c1))
				return NULL;
			c2 = term_read(0);
			if (TK_INT(c2))
				return NULL;
			return conf_digraph(c1, c2);
		default:
			if ((c & 0xc0) == 0xc0) {	/* utf-8 character */
				buf[0] = c;
				n = uc_len(buf);
				for (i = 1; i < n; i++)
					buf[i] = term_read(0);
				buf[n] = '\0';
				return buf;
			}
			return kmap_map(*kmap, c);
		}
		c = term_read(0);
	}
	return NULL;
}

#define led_info(buf) \
{ \
	led_att la; \
	la.s = NULL; \
	la.att = WH1 | SYN_BD | SYN_OWR; \
	sbuf *prev_attsb = led_attsb; \
	sbuf_make(led_attsb, sizeof(la) * 2) \
	for (i = uc_slen(buf) - 1; i >= 0; i--) { \
		la.off = *poff + i; \
		sbuf_mem(led_attsb, &la, sizeof(la)) \
	} \
	sbuf_str(sb, buf) \
	led_printparts(sb, pre, ps, *post, postn, poff); \
	sbuf_cut(sb, len) \
	sbuf_free(led_attsb) \
	led_attsb = prev_attsb; \
	c = term_read(TK_CTL('l')); \
	led_printparts(sb, pre, ps, *post, postn, poff); \
	goto noredraw; \
} \

static void led_redraw(char *cs, int r, int orow, int crow, int ctop, int flg)
{
	rstate++;
	for (int nl = 0; r < xrows; r++) {
		if (vi_lncol) {
			term_pos(r, 0);
			term_kill();
		}
		if (r >= orow-ctop && r < crow-ctop) {
			sbuf_smake(cb, 128)
			nl = dstrlen(cs, '\n');
			sbuf_mem(cb, cs, nl+!!cs[nl])
			sbuf_nul4(cb)
			rstate->s = NULL;
			led_crender(cb->s, r, vi_lncol, xleft, xleft + xcols - vi_lncol)
			free(cb->s);
			cs += nl+!!cs[nl];
			continue;
		}
		nl = r < crow-ctop ? r+ctop : (r-(crow-orow+!!(flg & 4)))+ctop;
		led_crender(lbuf_get(xb, nl) ? lbuf_get(xb, nl) : "~", r,
			vi_lncol, xleft, xleft + xcols - vi_lncol)
	}
	term_pos(crow - ctop, 0);
	rstate--;
}

void led_modeswap(void)
{
	preserve(int, xquit, xquit = 0;)
	preserve(int, texec, texec = 0;)
	preserve(int, xvis, xvis ^= 2;)
	preserve(int, xexec_dep, xexec_dep = 0;)
	if (xvis & 2)
		ex();
	else {
		syn_setft(xb_ft);
		vi(1);
	}
	if (xquit > 0 || (xquit < -256 && xquit >= -512))
		restore(xquit)
	else if (xquit < -512)
		xquit += 256;
	restore(texec)
	restore(xvis)
	restore(xexec_dep)
}

/* read a line from the terminal */
static int led_line(sbuf *sb, int ps, int pre, char **post, int postn, char **postref,
	int ai_max, int *poff, int *kmap, ins_state *is, int orow, int crow, int ctop, int flg)
{
	char *cs;
	int len, c, i;
	sbuf *reg;
	do {
		led_printparts(sb, pre, ps, *post, postn, poff);
		len = sb->s_n;
		c = term_read(TK_CTL('l'));
		noredraw:
		switch (c) {
		case TK_CTL('h'):
			c = 127;
		case 127:
			if (len - pre > 0)
				sbuf_cut(sb, led_lastchar(sb->s + pre) + pre)
			else
				return c;
			break;
		case TK_CTL('u'):
			sbuf_cut(sb, is->sug_pt > pre && len > is->sug_pt ? is->sug_pt : pre)
			break;
		case TK_CTL('w'):
			if (len - pre > 0)
				sbuf_cut(sb, led_lastword(sb->s + pre) + pre)
			else if (ai_max >= 0)
				return c;
			break;
		case TK_CTL('t'):
			cs = uc_dup(sb->s + ps);
			sbuf_cut(sb, ps)
			sbuf_chr(sb, '\t')
			sbuf_str(sb, cs)
			free(cs);
			pre++;
			break;
		case TK_CTL('d'):
			if (sb->s[ps] == ' ' || sb->s[ps] == '\t') {
				memmove(&sb->s[ps], &sb->s[ps+1], len - ps - 1);
				sb->s_n--;
				pre--;
			}
			break;
		case TK_CTL(']'):
		case TK_CTL('\\'):
			if (c == TK_CTL(']')) {
				if (is->p_reg < '/' || is->p_reg >= '9')
					is->p_reg = '/';
				while (is->p_reg < '9' && !ex_regget(++is->p_reg));
			} else {
				c = term_read(0);
				is->p_reg = c == TK_CTL('\\') ? 0 : c;
			}
			if (ex_regget(is->p_reg))
				led_info(ex_regget(is->p_reg)->s)
			continue;
		case TK_CTL('p'):
			if ((reg = ex_regget(is->p_reg)))
				sbuf_mem(sb, reg->s, reg->s_n)
			break;
		case TK_CTL('g'):
			if (!suggestsb) {
				sbuf_make(suggestsb, 1)
				sbuf_make(acsb, 1024)
				sbufn_chr(acsb, '\n')
			}
			file_index(xb);
			break;
		case TK_CTL('y'):
			led_done();
			suggestsb = NULL;
			break;
		case TK_CTL('r'):
			if (!suggestsb || !suggestsb->s_n)
				continue;
			if (!is->sug)
				is->sug = suggestsb->s;
			if (suggestsb->s_n == is->sug - suggestsb->s)
				is->sug--;
			for (i = 0; is->sug != suggestsb->s; is->sug--) {
				if (!*is->sug) {
					i++;
					if (i == 3) {
						is->sug++;
						goto redo_suggest;
					} else
						*is->sug = '\n';
				}
			}
			goto redo_suggest;
		case TK_CTL('z'):
			term_suspend();
			if (ai_max >= 0)
				led_redraw(sb->s, 0, orow, crow, ctop, flg);
			continue;
		case TK_CTL('x'):
			is->sug_pt = is->sug_pt == len ? -1 : len;
			char buf[100];
			itoa(is->sug_pt, buf);
			led_info(buf)
		case TK_CTL('n'):
			if (!suggestsb)
				continue;
			is->lsug = is->sug_pt >= 0 ? is->sug_pt : led_lastword(sb->s + pre) + pre;
			if (is->_sug) {
				if (suggestsb->s_n == is->sug - suggestsb->s)
					continue;
				redo_suggest:
				if (!(is->_sug = strchr(is->sug, '\n'))) {
					is->sug = suggestsb->s;
					goto lookup;
				}
				suggest:
				*is->_sug = '\0';
				sbuf_cut(sb, is->lsug)
				sbuf_str(sb, is->sug)
				is->sug = is->_sug+1;
				continue;
			}
			lookup:
			if (search(sb->s + is->lsug, len - is->lsug)) {
				is->sug = suggestsb->s;
				if (!(is->_sug = strchr(is->sug, '\n')))
					continue;
				goto suggest;
			}
			continue;
		case TK_CTL('b'):
			if (ai_max >= 0) {
				pac:;
				sbuf_nul(sb)
				int r = crow-ctop+1;
				if (is->sug)
					goto pac_;
				i = is->sug_pt >= 0 ? is->sug_pt : led_lastword(sb->s + pre) + pre;
				if (suggestsb && search(sb->s + i, sb->s_n - i)) {
					is->sug = suggestsb->s;
					pac_:;
					preserve(int, xtd, xtd = 2;)
					preserve(int, ftidx,)
					syn_setft(ac_ft);
					for (int left = 0; r < xrows; r++) {
						RS(2, led_crender(is->sug, r, 0, left, left+xcols))
						left += xcols;
						if (left >= rstates[2].pos[rstates[2].n])
							break;
					}
					restore(xtd)
					restore(ftidx)
					r++;
				}
				led_redraw(sb->s, r, orow, crow, ctop, flg);
				continue;
			}
			temp_pos(0, -1, 0, 0);
			temp_write(0, sb->s + pre);
			preserve(struct buf*, ex_buf,)
			int bidx = istempbuf(ex_buf) ? -1 : ex_buf - bufs;
			int pidx = ex_pbuf - bufs;
			preserve(int, texec, texec = 0;)
			preserve(int, xquit, xquit = 0;)
			preserve(int, ftidx,)
			temp_switch(0, 0);
			vi(1);
			exbuf_save(ex_buf)
			restore(texec)
			ex_pbuf = pidx >= xbufcur ? bufs : bufs + pidx;
			if (bidx >= 0)
				ex_buf = bidx >= xbufcur ? bufs : bufs + bidx;
			else
				restore(ex_buf)
			exbuf_load(ex_buf)
			syn_setft(xb_ft);
			vi(1); /* redraw past screen */
			restore(ftidx)
			term_pos(xrows, 0);
			if (xquit > 0 || (xquit < -256 && xquit >= -512))
				restore(xquit)
			else if (xquit < -512)
				xquit += 256;
			is->t_row = tempbufs[0].row;
		case TK_CTL('a'):
			is->t_row = is->t_row < -1 ? tempbufs[0].row : is->t_row;
			is->t_row += lbuf_len(tempbufs[0].lb);
			is->t_row = is->t_row % MAX(1, lbuf_len(tempbufs[0].lb));
			if ((cs = lbuf_get(tempbufs[0].lb, is->t_row--))) {
				sbuf_cut(sb, pre)
				sbuf_str(sb, cs)
				sb->s_n--;
			}
			break;
		case TK_CTL('l'):
			i = term_winch;
			term_done();
			term_init();
			if (ai_max >= 0)
				led_redraw(sb->s, 0, orow, crow, ctop, flg);
			else if (!i)
				term_clean();
			continue;
		case TK_CTL('o'): {
			if (!*postref)
				*postref = *post = uc_dup(*post);
			preserve(struct buf*, ex_buf,)
			int bidx = istempbuf(ex_buf) ? -1 : ex_buf - bufs;
			preserve(int, ftidx,)
			led_modeswap();
			restore(ftidx)
			if (bidx < 0) {
				if (ex_buf == tmpex_buf)
					continue;
				restore(ex_buf)
				exbuf_load(ex_buf)
			} else if (bidx != ex_buf - bufs && bidx < xbufcur) {
				ex_buf = bufs + bidx;
				exbuf_load(ex_buf)
			}
			continue; }
		default:
			if (c == '\n' || TK_INT(c))
				return c;
			if ((cs = led_read(kmap, c)))
				sbuf_str(sb, cs)
		}
		is->sug = NULL;
		is->_sug = NULL;
		if (ai_max >= 0 && xpac)
			goto pac;
	} while (!(flg & 2));
	return c;
}

int led_prompt(sbuf *sb, char *insert, int *kmap, ins_state *is, int ps, int flg)
{
	int n, key, off;
	char *post = "", *postref = post;
	ins_state _is;
	vi_lncol = 0;
	if (flg & 2) {
		n = ps;
		ps = 0;
	} else
		n = sb->s_n;
	if (insert)
		sbuf_str(sb, insert)
	if (!is) {
		ins_init(_is)
		is = &_is;
	}
	preserve(int, xleft, xleft = 0;)
	preserve(int, xtd, xtd = 2;)
	key = led_line(sb, ps, n, &post, 0, &postref, -1,
			&off, kmap, is, 0, xrow, xtop, flg);
	restore(xtd)
	restore(xleft)
	if (key == '\n' && flg & 1) {
		lbuf_dedup(tempbufs[0].lb, sb->s + n, sb->s_n - n)
		temp_pos(0, -1, 0, 0);
		temp_write(0, sb->s + n);
	}
	return key;
}

int led_input(sbuf *sb, char *post, int postn, int row, int flg, int *pren)
{
	int ai_max = 128 * xai;
	int n, key, ps = 0, crow = xrow, ctop = xtop;
	char *postref = NULL;
	ins_state is;
	while (1) {
		ins_init(is)
		key = led_line(sb, ps, sb->s_n, &post, postn, &postref,
			ai_max, &xoff, &xkmap, &is, row, crow, ctop, flg);
		if (key != '\n') {
			*pren = sb->s_n;
			if (!xled) {
				xoff = uc_slen(sb->s+ps);
				sbufn_str(sb, post)
			} else
				sb->s[*pren] = *post;
			free(postref);
			xrow = crow;
			return key;
		}
		sbuf_chr(sb, key)
		led_printparts(sb, -1, ps, "", 0, &xoff);
		term_chr('\n');
		term_room(1);
		crow++;
		n = ps;
		ps = sb->s_n;
		if (ai_max > 0) {	/* updating autoindent */
			for (; *post == ' ' || *post == '\t'; postn--)
				++post;
			int ai_new = n;
			while (sb->s[ai_new] == ' ' || sb->s[ai_new] == '\t')
				ai_new++;
			ai_new = ai_max > ai_new - n ? ai_new - n : ai_max;
			sbuf_mem(sb, sb->s+n, ai_new)
		}
	}
}

void led_done(void)
{
	if (suggestsb) {
		sbuf_free(suggestsb)
		sbuf_free(acsb)
	}
}
