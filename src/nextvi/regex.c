static int isword(const char *s)
{
	return uc_isalpha(*s) || uc_isdigit(*s) || s[0] == '_';
}

enum
{
	/* Instructions which consume input bytes */
	CHAR,
	CLASS,
	MATCH,
	ANY,
	/* Assert position */
	WBEG,
	WEND,
	BOL,
	EOL,
	LOOKAROUND,
	/* Other (special) instructions */
	SAVE,
	/* Instructions which take relative offset as arg */
	JMP,
	SPLIT,
	RSPLIT,
};

typedef struct rsub rsub;
struct rsub
{
	int ref;
	rsub *freesub;
	const char *sub[];
};

typedef struct rthread rthread;
struct rthread
{
	int *pc;
	rsub *sub;
};

#define INSERT_CODE(at, num, pc) \
if (code) \
	memmove(code + at + num, code + at, (pc - at)*sizeof(int)); \
pc += num;
#define REL(at, to) (to - at - 2)
#define EMIT(at, byte) (code ? (code[at] = byte) : at)
#define PC (prog->unilen)

static int re_sizecode(char *re, int *nsub, int *laidx, int flg);
static int reg_comp(rcode *prog, char *re, int nsubc, int laidx, int flg);

static void reg_free(rcode *p)
{
	for (int i = 0; i < p->laidx; i++)
		reg_free(p->la[i]);
	free(p->la);
	free(p);
}

static int compilecode(char *re_loc, rcode *prog, int sizecode, int flg)
{
	char *re = re_loc, *s, *p;
	int *code = sizecode ? NULL : prog->insts;
	int start = PC, term = PC, lb_start = 0;
	int alt_label = 0, c, l;
	int alt_stack[4096], altc = 0;
	int cap_stack[4096 * 5], capc = 0;

	while (*re) {
		switch (*re) {
		case '\\':
			re++;
			if (!*re)
				return -1; /* Trailing backslash */
			if (*re == '<' || *re == '>') {
				EMIT(PC++, *re == '<' ? WBEG : WEND);
				term = PC;
				break;
			}
		default:
			term = PC;
			uc_code(c, re, l)
			emit_char:
			if (flg & REG_ICASE && (unsigned int)c < 128)
				c = tolower(c);
			EMIT(PC++, CHAR);
			EMIT(PC++, c);
			break;
		case '.':
			term = PC;
			EMIT(PC++, ANY);
			break;
		case '[':;
			term = PC;
			int cnt, neq = *(++re) == '^';
			if (neq)
				re++;
			else if (*re != ']') {
				s = re + (*re == '\\');
				l = uc_len(s);
				if (l && s[l] == ']') {
					uc_code(c, s, l)
					re = s + l;  /* degrade a single character to CHAR */
					goto emit_char;
				}
			}
			PC += 3;
			for (cnt = 0; *re != ']'; cnt++) {
				if (*re == '\\')
					re++;
				uc_code(c, re, l)
				if (flg & REG_ICASE && (unsigned int)c < 128)
					c = tolower(c);
				EMIT(PC++, c);
				if (re[l] == '-' && re[l+1] != ']') {
					re += l + 1 + (re[l+1] == '\\');
					uc_code(c, re, l)
					if (flg & REG_ICASE && (unsigned int)c < 128)
						c = tolower(c);
				}
				EMIT(PC++, c);
				if (!l)
					return -1;
				re += l;
			}
			EMIT(term, CLASS);
			EMIT(term + 1, neq);
			EMIT(term + 2, cnt);
			break;
		case '(':;
			term = PC;
			int sub, sz, laidx, bal, la_static;
			if (re[1] == '?') {
				re += 2;
				if (*re == ':')
					goto non_capture;
				else if (*re == '#') {
					lb_start = atoi(re+1);
					if (!(re = strchr(re, ')')))
						return -1;
					break;
				} else if (*re != '=' && *re != '!' && *re != '>' && *re != '<')
					return -1;
				EMIT(PC++, LOOKAROUND);
				EMIT(PC++, *re == '=' ? 1 : *re == '!' ? -3 : *re == '>' ? 2 : -2);
				EMIT(PC++, prog->laidx);
				bal = 1;
				s = ++re;
				la_static = !(flg & REG_ICASE) && *s == '^';
				while (1) {
					if (!*s)
						return -1;
					else if (*s == '\\') {
						s++;
						if (code && (*s == '<' || *s == '>'))
							la_static = 0;
					} else if (*s == '(') {
						bal++;
						la_static = 0;
					} else if (*s == ')') {
						if (--bal == 0)
							break;
					} else if (code && la_static && strchr("|.*+?[{$", *s))
						la_static = 0;
					s += uc_len(s);
				}
				EMIT(PC++, la_static);
				EMIT(PC++, lb_start);
				if (code) {
					*s = '\0';
					if (la_static) {
						p = emalloc(sizeof(rcode) + s - re);
						prog->la[prog->laidx] = (rcode*)p;
						prog->la[prog->laidx]->laidx = 0;
						prog->la[prog->laidx]->la = NULL;
						for (p += sizeof(rcode), re++; re != s;) {
							if (*re == '\\')
								re += (s - re) > 1;
							*p++ = *re++;
						}
						EMIT(PC-2, p - (char*)(prog->la[prog->laidx]+1));
					} else {
						sz = re_sizecode(re, &sub, &laidx, REG_NOCAP) * sizeof(int);
						if (sz < 0)
							return -1;
						prog->la[prog->laidx] = emalloc(sizeof(rcode)+sz);
						if (reg_comp(prog->la[prog->laidx], re, sub, laidx, flg | REG_NOCAP)) {
							reg_free(prog->la[prog->laidx]);
							return -1;
						}
					}
					*s = ')';
				}
				prog->laidx++;
				re = s;
				break;
			}
			if (flg & REG_NOCAP) {
				non_capture:
				cap_stack[capc++] = 0;
			} else {
				sub = ++prog->sub;
				EMIT(PC++, SAVE);
				EMIT(PC++, sub);
				cap_stack[capc++] = 1;
			}
			cap_stack[capc++] = term;
			cap_stack[capc++] = alt_label;
			cap_stack[capc++] = start;
			cap_stack[capc++] = altc;
			alt_label = 0;
			start = PC;
			break;
		case ')':
			if (--capc-4 < 0)
				return -1;
			if (code && alt_label) {
				EMIT(alt_label, REL(alt_label, PC) + 1);
				int _altc = cap_stack[capc];
				for (int alts = altc; altc > _altc; altc--) {
					int at = alt_stack[_altc+alts-altc]+(altc-_altc)*2;
					EMIT(at, REL(at, PC) + 1);
				}
			}
			start = cap_stack[--capc];
			alt_label = cap_stack[--capc];
			term = cap_stack[--capc];
			if (cap_stack[--capc]) {
				EMIT(PC++, SAVE);
				EMIT(PC++, code[term+1] + prog->presub + 1);
			}
			break;
		case '{':;
			int i, maxcnt = 0, mincnt = 0, size = PC - term, nojmp = 0;
			re++;
			while (uc_isdigit(*re))
				mincnt = mincnt * 10 + *re++ - '0';
			if (*re == ',') {
				re++;
				if (*re == '}') {
					EMIT(PC, RSPLIT);
					EMIT(PC+1, REL(PC, PC - size));
					PC += 2;
					maxcnt = mincnt;
					nojmp = 1;
				}
				while (uc_isdigit(*re))
					maxcnt = maxcnt * 10 + *re++ - '0';
			} else
				maxcnt = mincnt;
			if (!mincnt && !maxcnt) {
				zcase:
				INSERT_CODE(term, 2, PC);
				EMIT(term, nojmp ? SPLIT : JMP);
				EMIT(term + 1, REL(term, PC));
				term = PC;
				break;
			}
			for (i = 0; i < mincnt-1; i++) {
				if (code)
					memcpy(&code[PC], &code[term], size*sizeof(int));
				PC += size;
			}
			if (!mincnt) {
				nojmp = 2;
				mincnt++;
			}
			for (i = maxcnt-mincnt; i > 0; i--) {
				EMIT(PC++, SPLIT);
				EMIT(PC++, REL(PC, PC+((size+2)*i)));
				if (code)
					memcpy(&code[PC], &code[term], size*sizeof(int));
				PC += size;
			}
			if (nojmp == 2)
				goto zcase;
			break;
		case '?':
			if (PC == term)
				return -1;
			INSERT_CODE(term, 2, PC);
			if (re[1] == '?') {
				EMIT(term, RSPLIT);
				re++;
			} else
				EMIT(term, SPLIT);
			EMIT(term + 1, REL(term, PC));
			term = PC;
			break;
		case '*':
			if (PC == term)
				return -1;
			INSERT_CODE(term, 2, PC);
			EMIT(PC, JMP);
			EMIT(PC + 1, REL(PC, term));
			PC += 2;
			if (re[1] == '?') {
				EMIT(term, RSPLIT);
				re++;
			} else
				EMIT(term, SPLIT);
			EMIT(term + 1, REL(term, PC));
			term = PC;
			break;
		case '+':
			if (PC == term)
				return -1;
			if (re[1] == '?') {
				EMIT(PC, SPLIT);
				re++;
			} else
				EMIT(PC, RSPLIT);
			EMIT(PC + 1, REL(PC, term));
			PC += 2;
			term = PC;
			break;
		case '|':
			if (alt_label)
				alt_stack[altc++] = alt_label;
			INSERT_CODE(start, 2, PC);
			EMIT(PC++, JMP);
			alt_label = PC++;
			EMIT(start, SPLIT);
			EMIT(start + 1, REL(start, PC));
			term = PC;
			break;
		case '^':
			EMIT(PC++, BOL);
			term = PC;
			break;
		case '$':
			EMIT(PC++, EOL);
			term = PC;
			break;
		}
		re += uc_len(re);
	}
	if (code && alt_label) {
		EMIT(alt_label, REL(alt_label, PC) + 1);
		for (int alts = altc; altc; altc--) {
			int at = alt_stack[alts-altc]+altc*2;
			EMIT(at, REL(at, PC) + 1);
		}
	}
	return capc ? -1 : 0;
}

static int re_sizecode(char *re, int *nsub, int *laidx, int flg)
{
	rcode dummyprog;
	dummyprog.unilen = 4;
	dummyprog.sub = 0;
	dummyprog.laidx = 0;
	int res = compilecode(re, &dummyprog, 1, flg);
	*nsub = dummyprog.sub;
	*laidx = dummyprog.laidx;
	return res < 0 ? res : dummyprog.unilen;
}

static int reg_comp(rcode *prog, char *re, int nsubc, int laidx, int flg)
{
	prog->len = 0;
	prog->unilen = 0;
	prog->sub = 0;
	prog->presub = nsubc;
	prog->splits = 0;
	prog->laidx = 0;
	prog->flg = flg;
	prog->la = laidx ? emalloc(laidx * sizeof(rcode*)) : NULL;
	if (compilecode(re, prog, 0, flg) < 0)
		return -1;
	int icnt = 0, scnt = SPLIT;
	for (int i = 0; i < prog->unilen; i++)
		switch (prog->insts[i]) {
		case LOOKAROUND:
			i += 4;
			break;
		case CLASS:
			i += prog->insts[i+2] * 2 + 2;
			icnt++;
			break;
		case SPLIT:
			prog->insts[i++] = scnt;
			scnt += 2;
			icnt++;
			break;
		case RSPLIT:
			prog->insts[i] = -scnt;
			scnt += 2;
		case JMP:
		case SAVE:
		case CHAR:
			i++;
		case ANY:
			icnt++;
		}
	prog->insts[prog->unilen++] = SAVE;
	prog->insts[prog->unilen++] = prog->sub + 1;
	prog->insts[prog->unilen++] = MATCH;
	prog->splits = MAX((scnt - SPLIT) / 2, 1);
	prog->len = icnt + 3;
	prog->presub = sizeof(rsub) + (sizeof(char*) * (nsubc + 1) * 2);
	prog->sub = prog->presub * (icnt + 6);
	prog->sparsesz = scnt;
	return 0;
}

#define _return(state) { if (eol_ch) utf8_length[eol_ch] = 1; return state; } \

#define newsub(init, copy) \
if (freesub) { \
	sub = freesub; freesub = sub->freesub; copy \
} else { \
	if (suboff == prog->sub) \
		suboff = 0; \
	sub = (rsub*)&nsubs[suboff]; \
	suboff += rsubsize; init \
} \

#define onlist(nn) \
if (sdense[spc] < sparsesz) \
	if (sdense[sdense[spc] << 1] == (unsigned int)spc) \
		deccheck(nn) \
sdense[spc] = sparsesz; \
sdense[sparsesz++ << 1] = spc; \

#define decref(csub) \
if (--csub->ref == 0) { \
	csub->freesub = freesub; \
	freesub = csub; \
} \

#define rec_check(nn) \
if (si) { \
	npc = pcs[--si]; \
	nsub = subs[si]; \
	goto rec##nn; \
} \

#define deccheck(nn) { decref(nsub) rec_check(nn) continue; } \

#define fastrec(nn, list, listidx) \
nsub->ref++; \
spc = *npc; \
if ((unsigned int)spc < WBEG) { \
	list[listidx].sub = nsub; \
	list[listidx++].pc = npc; \
	npc = pcs[si]; \
	goto rec##nn; \
} \
subs[si++] = nsub; \
goto next##nn; \

#define saveclist() \
if (npc[1] > (nsubc >> 1) && nsub->ref > 1) { \
	nsub->ref--; \
	newsub(memcpy(sub->sub, nsub->sub, osubp);, \
	memcpy(sub->sub, nsub->sub, osubp >> 1);) \
	nsub = sub; \
	nsub->ref = 1; \
} \

#define savenlist() \
if (nsub->ref > 1) { \
	nsub->ref--; \
	newsub(,) \
	memcpy(sub->sub, nsub->sub, osubp); \
	nsub = sub; \
	nsub->ref = 1; \
} \

#define clistmatch(n)
#define nlistmatch(n) \
if (spc == MATCH) \
	for (i++; i < clistidx; i++) { \
		npc = clist[i].pc; \
		nsub = clist[i].sub; \
		if (*npc == MATCH) \
			goto matched##n; \
		decref(nsub) \
	} \

#define addthread(n, nn, list, listidx) \
rec##nn: \
spc = *npc; \
if ((unsigned int)spc < WBEG) { \
	list[listidx].sub = nsub; \
	list[listidx++].pc = npc; \
	rec_check(nn) \
	list##match(n) \
	continue; \
} \
next##nn: \
if (spc > JMP) { \
	onlist(nn) \
	npc += 2; \
	pcs[si] = npc + npc[-1]; \
	fastrec(nn, list, listidx) \
} else if (spc == SAVE) { \
	save##list() \
	nsub->sub[npc[1]] = _sp; \
	npc += 2; goto rec##nn; \
} else if (spc == WBEG) { \
	if (((sp != s || sp != _sp) && isword(sp)) \
			|| !isword(_sp)) \
		deccheck(nn) \
	npc++; goto rec##nn; \
} else if (spc < 0) { \
	spc = -spc; \
	onlist(nn) \
	npc += 2; \
	pcs[si] = npc; \
	npc += npc[-1]; \
	fastrec(nn, list, listidx) \
} else if (spc == WEND) { \
	if (isword(_sp)) \
		deccheck(nn) \
	npc++; goto rec##nn; \
} else if (spc == EOL) { \
	if (flg & REG_NOTEOL || *_sp != eol_ch) \
		deccheck(nn) \
	npc++; goto rec##nn; \
} else if (spc == JMP) { \
	npc += 2 + npc[1]; \
	goto rec##nn; \
} else if (spc == LOOKAROUND) { \
	if ((npc[1] & 3) < 2) \
		s0 = _sp; \
	else if (npc[4] < 0) \
		s0 = s; \
	else if (npc[4]) { \
		s0 = _sp - npc[4]; \
		if (s0 < s) { \
			cnt = 0; \
			goto out##nn; \
		} \
	} else \
		s0 = sp; \
	j = npc[2]; \
	if (npc[3]) { \
		s1 = (char*)(prog->la[j]+1); \
		for (j = npc[3], cnt = 0; cnt < j && s0[cnt] == s1[cnt]; cnt++); \
		cnt = cnt == j; \
	} else if (!lb[j] || s0 > lb[j]) { \
		cnt = re_pikevm(prog->la[j], s0, _subp, 2, 0); \
		lb[j] = cnt ? _subp[0] : NULL; \
	} else \
		cnt = !!lb[j]; \
	out##nn: \
	if (npc[1] * ((cnt << 1) - 1) < 0) \
		deccheck(nn) \
	npc += 5; goto rec##nn; \
} else { \
	if (flg & REG_NOTBOL || _sp != s) { \
		if (!si && !clistidx) \
			_return(0) \
		deccheck(nn) \
	} \
	npc++; goto rec##nn; \
} \

#define swaplist() \
tmp = clist; \
clist = nlist; \
nlist = tmp; \
clistidx = nlistidx; \

#define deccont() { decref(nsub) continue; }

#define match(n, cpn) \
for (;; sp = _sp) { \
	uc_code(c, sp, i) cpn \
	_sp = sp+i; \
	nlistidx = 0, sparsesz = 0; \
	for (i = 0; i < clistidx; i++) { \
		npc = clist[i].pc; \
		nsub = clist[i].sub; \
		spc = *npc; \
		if (spc == CHAR) { \
			if (c != npc[1]) \
				deccont() \
			npc += 2; \
		} else if (spc == CLASS) { \
			pc = npc+1; \
			cnt = pc[1]; \
			for (; cnt > 0; cnt--) { \
				pc += 2; \
				if (c >= *pc && c <= pc[1]) \
					cnt = -1; \
			} \
			if (!(cnt || npc[1]) || (cnt < 0 && npc[1])) \
				deccont() \
			npc += npc[2] * 2 + 3; \
		} else if (spc == MATCH) { \
			matched##n: \
			nlist[nlistidx++].pc = &mcont; \
			if (npc != &mcont) { \
				if (matched) \
					decref(matched) \
				matched = nsub; \
			} \
			if (sp == _sp || nlistidx == 1) { \
				for (i = 0; i < nsubc; i+=2) { \
					subp[i] = matched->sub[i >> 1]; \
					subp[i+1] = matched->sub[(nsubc >> 1) + (i >> 1)]; \
				} \
				_return(1) \
			} \
			swaplist() \
			goto _continue##n; \
		} else \
			npc++; \
		addthread(n, 2##n, nlist, nlistidx) \
	} \
	if (sp == _sp) \
		break; \
	swaplist() \
	jmp_start##n: \
	newsub(memset(sub->sub, 0, osubp);,) \
	sub->ref = 1; \
	sub->sub[0] = _sp; \
	npc = insts; nsub = sub; \
	addthread(n, 1##n, clist, clistidx) \
	_continue##n:; \
} \
_return(0) \

static int re_pikevm(rcode *prog, const char *s, const char **subp, int nsubc, int flg)
{
	if (!*s)
		return 0;
	flg = prog->flg | flg;
	const char *sp = s, *_sp = s, *s0, *s1;
	int *pcs[prog->splits], *npc, *pc, *insts = prog->insts;
	rsub *subs[prog->splits];
	rsub *nsub, *sub, *matched = NULL, *freesub = NULL;
	rthread _clist[prog->len], _nlist[prog->len];
	rthread *clist = _clist, *nlist = _nlist, *tmp;
	const char *_subp[2], *lb[prog->laidx+1];
	int rsubsize = prog->presub, suboff = 0;
	int cnt, spc, i, c, j, osubp = nsubc * sizeof(char*);
	int si = 0, clistidx = 0, nlistidx, mcont = MATCH;
	int eol_ch = flg & REG_NEWLINE ? '\n' : 0;
	unsigned int sdense[prog->sparsesz], sparsesz = 0;
	char nsubs[prog->sub];
	for (i = 0; i < prog->laidx; i++)
		lb[i] = NULL;
	if (eol_ch)
		utf8_length[eol_ch] = 0;
	if (flg & REG_ICASE)
		goto jmp_start1;
	goto jmp_start2;
	match(1, if ((unsigned int)c < 128) c = tolower(c);)
	match(2,)
}

static int re_groupcount(char *s)
{
	int n;
	for (n = 0; *s; s++)
		if (s[0] == '\\' && s[1])
			s++;
		else if (s[0] == '(' && s[1] != '?')
			n++;
	return n;
}

void rset_free(rset *rs)
{
	if (!rs)
		return;
	reg_free(rs->regex);
	free(rs);
}

rset *rset_make(int n, char **re, int flg)
{
	int i, laidx, sz, nsubc, c = 0;
	rset *rs = emalloc(sizeof(*rs) + (((n + 1) * sizeof(rs->grp[0])) * 2));
	rs->grp = (int*)(rs + 1);
	rs->grpnsubc = rs->grp + n + 1;
	sbuf_smake(sb, 1024)
	rs->n = n;
	for (i = 0; i < n; i++)
		if (!re[i])
			c++;
	nsubc = ((n - c) > 1) * 2;
	for (i = 0; i < n; i++) {
		if (!re[i]) {
			rs->grp[i] = -2;
			continue;
		}
		if (sb->s_n > 0)
			sbuf_chr(sb, '|')
		if ((n - c) > 1)
			sbuf_chr(sb, '(')
		sbuf_str(sb, re[i])
		if ((n - c) > 1)
			sbuf_chr(sb, ')')
		rs->grp[i] = nsubc;
		rs->grpnsubc[i] = flg & REG_NOCAP ? 2 : (re_groupcount(re[i]) + 1) * 2;
		nsubc += rs->grpnsubc[i];
	}
	sbuf_nul4(sb)
	sz = re_sizecode(sb->s, &nsubc, &laidx, flg & REG_NOCAP ? REG_NOCAP : 0);
	if (sz > 0) {
		rs->regex = emalloc(sizeof(rcode) + (sz * sizeof(int)));
		if (!reg_comp(rs->regex, sb->s, nsubc, laidx, flg)) {
			rs->nsubc = (nsubc + 1) * 2;
			free(sb->s);
			return rs;
		}
		reg_free(rs->regex);
	}
	free(rs);
	free(sb->s);
	return NULL;
}

/* return the index of the matching regular expression or -1 if none matches */
int rset_find(rset *rs, char *s, int *grps, int flg)
{
	const char *subs[rs->nsubc+2];
	const char **sub = subs+2;
	if (re_pikevm(rs->regex, s, sub, rs->nsubc, flg)) {
		subs[1] = NULL; /* make sure sub[-1] never matches */
		for (int i = rs->n-1; i >= 0; i--) {
			if (sub[rs->grp[i] + 1]) {
				int n = grps ? rs->grpnsubc[i] : 0;
				for (int gi = 0; gi < n; gi += 2) {
					int grp = rs->grp[i] + gi;
					if (sub[grp] && sub[grp + 1]) {
						grps[gi] = sub[grp] - s;
						grps[gi + 1] = sub[grp + 1] - s;
					} else {
						grps[gi] = -1;
						grps[gi + 1] = -1;
					}
				}
				return i;
			}
		}
	}
	return -1;
}

int rset_match(rset *rs, char *s, int flg)
{
	return re_pikevm(rs->regex, s, NULL, 0, flg);
}
