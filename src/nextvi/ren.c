static rset *dir_rslr;	/* pattern of marks for left-to-right strings */
static rset *dir_rsrl;	/* pattern of marks for right-to-left strings */
static rset *dir_rsctx;	/* direction context patterns */

static void dir_reverse(int *ord, int beg, int end)
{
	end--;
	while (beg < end) {
		int tmp = ord[beg];
		ord[beg] = ord[end];
		ord[end] = tmp;
		beg++;
		end--;
	}
}

/* reorder the characters based on direction marks and characters */
static int dir_reorder(char *s, char *se, int *ord, int end, int dir)
{
	rset *rs = dir < 0 ? dir_rsrl : dir_rslr;
	int beg = 0, off, c_beg, c_end;
	int subs[LEN(dmarks[0].dir) * 2], found, i;
	int flg = se > s && se[-1] == '\n' ? REG_NEWLINE : 0;
	while (se > s && (found = rset_find(rs, s, subs, flg)) >= 0) {
		for (i = 0; i < end; i++)
			ord[i] = i;
		end = -1;
		c_end = 0;
		for (i = 0; i < rs->grpnsubc[found]; i += 2) {
			off = subs[i];
			if (off < 0 || dmarks[found].dir[i >> 1] >= 0)
				continue;
			c_beg = uc_off(s, off);
			c_end = c_beg + uc_off(s + off, subs[i + 1] - off);
			off = subs[i + 1];
			dir_reverse(ord, beg+c_beg, beg+c_end);
		}
		beg += c_end ? c_end : 1;
		s += c_end ? off : 1;
	}
	return end < 0;
}

/* return the direction context of the given line */
int dir_context(char *s)
{
	int found;
	if (xtd > +1)
		return +1;
	if (xtd < -1)
		return -1;
	if (dir_rsctx && s)
		if ((found = rset_find(dir_rsctx, s, NULL, 0)) >= 0)
			return dctxs[found].dir;
	return xtd < 0 ? -1 : +1;
}

void dir_init(void)
{
	char *relr[128];
	char *rerl[128];
	char *ctx[128];
	int i;
	for (i = 0; i < dmarkslen; i++) {
		relr[i] = dmarks[i].ctx >= 0 ? dmarks[i].pat : NULL;
		rerl[i] = dmarks[i].ctx <= 0 ? dmarks[i].pat : NULL;
	}
	dir_rslr = rset_make(i, relr, 0);
	dir_rsrl = rset_make(i, rerl, 0);
	for (i = 0; i < dctxlen; i++)
		ctx[i] = dctxs[i].pat;
	dir_rsctx = rset_make(i, ctx, 0);
}

static int ren_cwid(char *s, int pos)
{
	if (s[0] == '\t')
		return xts ? xts - (pos % xts) : 0;
	if (s[0] == '\n')
		return 1;
	int c, l; uc_code(c, s, l)
	for (int i = 0; i < phlen; i++)
		if (c >= ph[i].cp[0] && c <= ph[i].cp[1] && l == ph[i].l)
			return ph[i].wid;
	return uc_wid(c);
}

ren_state rstates[3]; /* 0 = current line, 1 = all other lines, 2 = aux rendering */
ren_state *rstate = rstates;

/* specify the screen position of the characters in s */
ren_state *ren_position(char *s)
{
	if (rstate->s == s)
		return rstate;
	else if (rstate->col) {
		free(rstate->col - 2);
		free(rstate->pos);
	}
	rstate->s = s;
	rstate->ctx = dir_context(s);
	unsigned int n, max, l;
	char *ss = s;
	if (xlim >= 0 && rstate == rstates+1) {
		max = (unsigned int)xlim;
		for (n = 0; n < max && (l = uc_len(ss)); n++)
			ss += l;
		rstate->holelen = uc_len(ss);
		memcpy(rstate->nulhole, ss, rstate->holelen);
		memset(ss, 0, rstate->holelen);
	} else
		for (n = 0; (l = uc_len(ss)); n++)
			ss += l;
	unsigned int b = n + 1, c = 2, i;
	int cpos = 0, wid, *col;
	int *pos = emalloc((b * 2 * sizeof(pos[0])) + b * sizeof(char*));
	int *off = &pos[b];
	char **chrs = (char**)&off[b];
	if (xorder && dir_reorder(s, ss, off, n, rstate->ctx)) {
		for (i = 0; i < b; i++) {
			chrs[i] = s;
			s += uc_len(s);
		}
		int *wids = emalloc(n * sizeof(wids[0]));
		for (i = 0; i < n; i++) {
			wid = ren_cwid(chrs[off[i]], cpos);
			pos[off[i]] = cpos;
			wids[off[i]] = wid;
			cpos += wid;
		}
		pos[n] = cpos;
		col = emalloc((cpos + 2) * sizeof(col[0]));
		for (i = 0; i < n; i++) {
			wid = wids[off[i]];
			while (wid--)
				col[c++] = off[i];
		}
		memcpy(off, wids, n * sizeof(wids[0]));
		free(wids);
	} else {
		for (i = 0; i < n; i++) {
			chrs[i] = s;
			pos[i] = cpos;
			cpos += ren_cwid(s, cpos);
			s += uc_len(s);
		}
		chrs[n] = s;
		pos[n] = cpos;
		col = emalloc((cpos + 2) * sizeof(col[0]));
		for (i = 0; i < n; i++) {
			wid = pos[i+1] - pos[i];
			off[i] = wid;
			while (wid--)
				col[c++] = i;
		}
	}
	off[n] = 0;
	col[0] = n;
	col[1] = n;
	rstate->wid = off;
	rstate->cmax = cpos - 1;
	rstate->col = col + 2;
	rstate->pos = pos;
	rstate->chrs = chrs;
	rstate->n = n;
	return rstate;
}

/* convert character offset to visual position */
int ren_pos(char *s, int off)
{
	ren_state *r = ren_position(s);
	return off < r->n ? r->pos[off] : 0;
}

/* convert visual position to character offset */
int ren_off(char *s, int p)
{
	ren_state *r = ren_position(s);
	return r->col[p < r->cmax ? p : r->cmax];
}

/* adjust cursor position */
int ren_cursor(char *s, int p)
{
	if (!s)
		return 0;
	ren_state *r = ren_position(s);
	if (p >= r->cmax)
		p = r->cmax - (*r->chrs[r->col[r->cmax]] == '\n');
	int i = r->col[p];
	return r->pos[i] + r->wid[i] - 1;
}

/* return an offset before EOL */
int ren_noeol(char *s, int o)
{
	if (!s)
		return 0;
	ren_state *r = ren_position(s);
	o = MAX(0, o >= r->n ? r->n - 1 : o);
	return o - (o > 0 && *r->chrs[o] == '\n');
}

/* the visual position of the next character */
int ren_next(char *s, int p, int dir)
{
	ren_state *r = ren_position(s);
	if (p+dir < 0 || p > r->cmax)
		return r->pos[r->col[r->cmax]];
	int i = r->col[p];
	if (r->wid[i] > 1 && dir > 0)
		return r->pos[i] + r->wid[i];
	return r->pos[i] + dir;
}

char *ren_translate(char *s, char *ln)
{
	if (s[0] == '\t' || s[0] == '\n')
		return NULL;
	int c, l; uc_code(c, s, l)
	for (int i = 0; i < phlen; i++)
		if (c >= ph[i].cp[0] && c <= ph[i].cp[1] && l == ph[i].l)
			return ph[i].d;
	if (l == 1)
		return NULL;
	if (uc_acomb(c)) {
		static char buf[16] = "ـ";
		*((char*)memcpy(buf+2, s, l)+l) = '\0';
		return buf;
	}
	if (uc_isbell(c))
		return "�";
	return xshape ? uc_shape(ln, s, c) : NULL;
}

/* mapping filetypes to regular expression sets */
struct ftmap {
	int setbidx;
	int seteidx;
	char *ft;
	rset *rs;
};
static struct ftmap *ftmap;
static int ftmidx;
static rset *syn_ftrs;
static int blockatt, blockflg, blockdep;
int ftidx;
int syn_scdirl;
int syn_blockhl;

static int syn_initft(int fti, int n, char *name, int flg)
{
	if (fti >= ftmidx)
		ftmap = erealloc(ftmap, (fti + 1) * sizeof(*ftmap));
	int i = n, set = hls[i].set;
	char *pats[hlslen];
	for (; i < hlslen && hls[i].ft == name && hls[i].set == set; i++)
		pats[i - n] = hls[i].pat;
	ftmap[fti].setbidx = n;
	ftmap[fti].ft = name;
	ftmap[fti].rs = rset_make(i - n, pats, flg);
	ftmap[fti].seteidx = i;
	return i < hlslen && hls[i].ft == name && hls[i].set != set;
}

char *syn_setft(char *ft)
{
	int i;
	if (ftmidx)
		for (i = 1; i < 4; i++)
			syn_addhl(NULL, i);
	for (i = 0; i < ftmidx; i++)
		if (ft == ftmap[i].ft) {
			ftidx = i;
			return ftmap[ftidx].ft;
		}
	for (i = 0; i < hlslen; i++)
		if (ft == hls[i].ft) {
			default_hl:
			ftidx = ftmidx;
			while (syn_initft(ftmidx, i, hls[i].ft, 0))
				i = ftmap[ftmidx++].seteidx;
			ftmidx++;
			return ftmap[ftidx].ft;
		}
	if (ftmidx)
		return NULL;
	i = 0;
	goto default_hl;
}

void syn_scdir(int scdir)
{
	if (!scdir || abs(scdir) > xrows || (syn_scdirl > 0) != (scdir > 0)) {
		syn_scdirl = scdir;
		syn_blockhl = -1;
		blockdep = 0;
	}
}

int syn_merge(int old, int new)
{
	if (new & SYN_OWR)
		return new & ~SYN_OWR;
	int fg = SYN_FGSET(new) ? SYN_FG(new) : SYN_FG(old);
	int bg = SYN_BGSET(new) ? SYN_BG(new) : SYN_BG(old);
	int flg = ((old | new) & SYN_FLG) | (new & SYN_MK);
	return flg | (bg << 8) | fg;
}

static int syn_tatt(int *att, int a, int pb)
{
	if (SYN_SET(BATT, a) && pb && (!*att || !SYN_SET(BP, blockflg)))
		att = &blockatt;
	return (*att & 0xffff) == (a & 0xffff);
}

void syn_highlight(int *att, char *s, int n)
{
	int fti = ftidx, blockhl = syn_blockhl, blockcont = -1;
	re:;
	rset *rs = ftmap[fti].rs;
	int subs[rs->nsubc], *catt, *iatt, sl, c;
	int cend, sidx = 0, flg = 0, hl, j, i, ii;
	while ((sl = rset_find(rs, s + sidx, subs, flg)) >= 0) {
		cend = 1;
		hl = sl + ftmap[fti].setbidx;
		sl = rs->grpnsubc[sl];
		catt = hls[hl].att;
		for (i = 0, ii = i; ii < sl; ii += 2) {
			int inc = 1;
			if (subs[ii] < 0 || SYN_SET(IGN, catt[i])) {
				skip:
				if (SYN_SET(ATT, catt[i]))
					inc += catt[i + 1] + 1;
				if (SYN_SET(OATT, catt[i]))
					inc += catt[i + inc] + 1;
				if (SYN_SET(BLK, catt[i]))
					inc++;
				i += inc;
				continue;
			}
			cend = MAX(cend, subs[ii + 1]);
			if (SYN_SET(SKIP, catt[i]))
				goto skip;
			int beg = uc_off(s, sidx + subs[ii]);
			int end = beg + uc_off(s + sidx + subs[ii], subs[ii + 1] - subs[ii]);
			if (SYN_SET(ATT, catt[i])) {
				int pb = blockhl >= 0 && syn_blockhl >= 0;
				iatt = &catt[i + 1];
				c = *iatt;
				inc += c + 1;
				if (SYN_SET(ATT, catt[i]) == SYN_ATT) {
					for (j = beg; c && j < end; j++)
						for (c = *iatt; c && !syn_tatt(att + j, iatt[c], pb); c--);
				} else if (SYN_SET(SATT, catt[i]))
					for (; c && !syn_tatt(att + beg, iatt[c], pb); c--);
				else if (SYN_SET(EATT, catt[i]))
					for (; c && !syn_tatt(att + MAX(0, end-1), iatt[c], pb); c--);
				if (!c)
					break;
			}
			if (SYN_SET(OATT, catt[i])) {
				int pb = blockhl >= 0 && syn_blockhl >= 0;
				iatt = &catt[i + inc];
				inc += *iatt + 1;
				for (j = beg; j < end; j++) {
					for (c = *iatt; c && !syn_tatt(att + j, iatt[c], pb); c--);
					if (c)
						att[j] = syn_merge(att[j], catt[i]);
				}
			} else
				for (j = beg; j < end; j++)
					att[j] = syn_merge(att[j], catt[i]);
			if (SYN_SET(BLK, catt[i])) {
				iatt = &catt[i + inc];
				inc++;
				j = SYN_SET(BSDP, *iatt) || !!SYN_SET(BSD, *iatt) == (syn_scdirl > 0);
				c = SYN_SET(BEDP, *iatt) || !!SYN_SET(BED, *iatt) == (syn_scdirl > 0);
				if (syn_blockhl == hl && SYN_SET(BN, *iatt) && j) {
					blockdep++;
				} else if (syn_blockhl == hl && SYN_SET(BE, *iatt) && c) {
					if (blockdep) {
						blockdep--;
					} else {
						blockcont = -1;
						syn_blockhl = blockcont;
					}
				} else if (syn_blockhl < 0 && SYN_SET(BS, *iatt)) {
					if (j && blockcont <= 0) {
						blockflg = *iatt;
						blockatt = catt[0];
						syn_blockhl = hl;
					}
					blockcont = hl;
				}
			}
			i += inc;
		}
		sidx += cend;
		flg = REG_NOTBOL;
	}
	fti++;
	if (ftmidx > fti && ftmap[fti-1].ft == ftmap[fti].ft)
		goto re;
	if (syn_blockhl < 0 || blockhl < 0)
		return;
	for (j = 0; j < n; j++)
		if (!att[j] || !SYN_SET(BP, blockflg))
			att[j] = blockatt;
}

char *syn_filetype(char *path)
{
	int hl = rset_find(syn_ftrs, path, NULL, 0);
	return hl >= 0 && hl < ftslen ? fts[hl].ft : hls[0].ft;
}

void syn_reloadft(int hl, int flg)
{
	if (hl >= 0) {
		int fti = ftidx;
		while (fti < ftmidx - 1 && hl >= ftmap[fti].seteidx)
			fti++;
		rset *rs = ftmap[fti].rs;
		syn_initft(fti, ftmap[fti].setbidx, ftmap[fti].ft, flg);
		if (!ftmap[fti].rs)
			ftmap[fti].rs = rs;
		else
			rset_free(rs);
	}
}

int syn_findhl(int id)
{
	int i = ftmap[ftidx].setbidx;
	char *name = ftmap[ftidx].ft;
	for (; i < hlslen && hls[i].ft == name; i++)
		if (hls[i].id == id)
			return i;
	return -1;
}

int syn_addhl(char *reg, int id)
{
	int ret = syn_findhl(id);
	if (ret >= 0)
		hls[ret].pat = reg;
	return ret;
}

void syn_init(void)
{
	char *pats[ftslen];
	int i = 0;
	for (; i < ftslen; i++)
		pats[i] = fts[i].pat;
	syn_ftrs = rset_make(i, pats, 0);
}
