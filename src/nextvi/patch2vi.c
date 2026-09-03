/*
 * patch2vi - turn a unified diff into a /bin/sh script driving nextvi's ex
 * engine, and back.
 *
 * The script applies the patch in raw ex mode, anchored by line number (-a)
 * or search pattern (-r); the command separator and the escape byte are
 * picked from the bytes the patch does not use, and the original diff is
 * stored after the script's "exit 0", so a generated script regenerates
 * without the diff at hand.
 *
 * Nextvi is embedded whole: vi.c (and through it every editor module) is
 * compiled into this translation unit, build_patch2vi.sh renaming nextvi's
 * main() to nextvi_main() for the build. No session ever writes a buffer
 * back; quitting is what emits, to stdout or (-o) atomically onto a named
 * file.
 */
#include "vi.c"

/* nextvi's own main(), renamed for this build by build_patch2vi.sh; -E
 * runs a whole editing session through it, flags, EXINIT and all */
int nextvi_main(int argc, char *argv[]);

/* Drops -o's temporary twin, once out_redirect() has made one; a no-op
 * before that. Called on the failure exits patch2vi itself notices. */
static void out_cleanup(void);

/* Initial capacity for sbufs holding one line-ish string; they grow. */
#define SB_INIT 512

/* Grow (arr, cap) so slot n exists and is zeroed; the caller fills it and
 * bumps n. Every table here grows this way - no fixed limits. */
#define ARR_PUSH(arr, n, cap) \
{ \
	if ((n) >= (cap)) { \
		(cap) = (cap) ? (cap) * 2 : 8; \
		(arr) = erealloc((arr), (cap) * sizeof(*(arr))); \
	} \
	memset(&(arr)[n], 0, sizeof(*(arr))); \
} \

typedef struct {
	int type;       /* 'd'=delete, 'a'=add, 'c'=context */
	int oline;      /* line number in original file */
	char *text;     /* line content (all op types: adds insert it,
			 * deletes/context anchor searches and diffs) */
	int hunk_lo;    /* 1-based first original line of the enclosing @@ hunk */
	int hunk_hi;    /* 1-based last original line of the enclosing @@ hunk */
} op_t;

struct group_s;

typedef struct {
	char *path;
	op_t *ops;
	int nops, ops_cap;
	struct group_s *groups;  /* heap-allocated, set by build_file_groups */
	int ngroups;
	int is_new;              /* patch creates this file (--- /dev/null) */
	char *orig_path;         /* "---" path, holds pre-patch content (file-aware) */
} file_patch_t;

static file_patch_t *files;
static int nfiles, files_cap;
static const char *cur_file_path;  /* set per-file for error messages */
static int relative_mode = 1;  /* 1=relative search (the default, -r),
				* 0=absolute line numbers (-a); last of the
				* two on the command line wins */
static int absolute_opt;	/* -a seen: -E leaves the re-emitted host
				* patch absolute instead of re-anchoring
				* it; stored blocks stay search anchored */
/* patch (or previously generated script) path, NULL = stdin */
static const char *input_file;
static const char *end_tag_rd = "=== END ===";
static const char *end_tag_wr = "=== END ===";

/* The emit layer builds everything in memory. Function (not macro) wrappers
 * around the sbuf appenders let call sites sit in unbraced if/else bodies. */
static void sb_str(sbuf *sb, const char *s)
{
	sbuf_str(sb, s)
}

static void sb_chr(sbuf *sb, int c)
{
	sbuf_chr(sb, c)
}

static void sb_printf(sbuf *sb, const char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (sb->s_n + n + 1 >= sb->s_sz) {
		sb->s_sz = NEXTSZ(sb->s_n, n + 1) + 1;
		sb->s = erealloc(sb->s, sb->s_sz);
	}
	va_start(ap, fmt);
	vsnprintf(sb->s + sb->s_n, n + 1, fmt, ap);
	va_end(ap);
	sb->s_n += n;
}

/* f> anchor search strategies, one per pattern slot, tried strict-to-loose,
 * first match wins: NPAT exact ones (default_pat_lines), then the
 * file-validated relaxed windows - fuzz (gen_fuzz_windows), the :grp-capture
 * window 7 (gen_grp_window) and the straddle windows 8 and 9 (gen_win_window,
 * skip 0 and 1). */
#define NPAT 5
#define NFUZZ 1   /* max file-validated fuzzed candidates per group (loosest kept) */
#define NGRP 1    /* file-validated :grp-capture window (TEXT.*? + last captured) */
#define NWIN 1    /* file-validated "top.*(bottom)" straddle window (pattern 8) */
#define NWIN2 1   /* second straddle window, anchors farther out (pattern 9) */
#define GRP_SLOT (NPAT + NFUZZ)         /* 0-based slot index of the grp window */
#define WIN_SLOT (NPAT + NFUZZ + NGRP)  /* 0-based slot index of the straddle window */
#define WIN2_SLOT (NPAT + NFUZZ + NGRP + NWIN)  /* 0-based slot index of the farther straddle window */
#define NSEARCH (NPAT + NFUZZ + NGRP + NWIN + NWIN2)  /* must stay <= 9: section numbers are 1 digit */

/* Cursor save/restore scratch for the global (mode 3) searches; edit marks
 * start at 1. */
#define WIN_SAVE_MARK 0

/*
 * SEARCH MODES. Every phase-1 search carries one; it follows from the pattern's
 * shape alone: 1 for a single line, 0 otherwise, and the window generators
 * pick their own. emit_search_setup writes the form each implies:
 *   0  "%f>" over the find register cache - the buffer yanked once after open,
 *      where the whole file is one string, so a multi-line window's newlines
 *      are visible to the regex; f+ resumes one char past the previous match.
 *   1  ".,$f>" over the live buffer from the current line's first column, with
 *      ^...$ per-line anchors to disambiguate repeated text ("fr" drops the
 *      cache for the search, "fr 98" puts it back).
 *   2  mode 0 bracketed with "grp 1 .. grp 0", so the find lands on the
 *      captured group (pattern 7's absorbing window).
 *   3  mode 2 run globally: the cursor is saved to WIN_SAVE_MARK, reset to the
 *      top and restored after (patterns 8 and 9's straddle windows).
 */

/* THE IDENTITY GATE. A compat block (the fix for a collision with its src=
 * origins) fires iff every origin named in its label is in the applied set,
 * the basenames carried in $P2VI_PATCH: the block is gated on the identity of
 * what already ran, never on the text it left behind. A script inherits the
 * set from its caller, appends itself, and hands the remaining arguments to
 * the next script, so every level of a chain sees exactly what preceded it.
 * $P2VI_PATCH is the escape hatch for trees that did not start clean (a change
 * applied by hand, absorbed upstream, or arriving pre-patched) and is what -C
 * asserts while deriving a new block.
 *
 * The decision itself is the editor's: the set goes into a register whole and
 * every gate searches it for its own origins, so the shell contributes one
 * variable and no logic. -e and the replay path set that same variable from
 * their own accumulated set, which is why the two paths cannot drift - there
 * is only one implementation of the rule, and it is in the ex body. */

/* Set while the replay session is the compat one, so replay_blocks() snapshots
 * the post-origin baseline right before handing over to the user. */
static int compat_capturing;
/* Set while a compat block's groups are generated/emitted: exact strategies
 * only, since the file-validated generators would read the pre-origin file -
 * the wrong text for a block derived on top of it. */
static int compat_building;
static int compat_mode;			/* -C: derive a post-only compat patch */
/* -E's optional block selector: the section register naming the one stored
 * compat block this run rebuilds instead of the base patch; without it the
 * base patch is what -E updates and the blocks stand as stored. */
static int amend_sel = -1;
/* The scripts it is derived against, in replay order: -C repeats, one per
 * origin, and one identity gate tests all of them at once. One origin is the
 * ordinary case and behaves exactly as the single -C always did. */
static const char **compat_origins;
static int ncompat_origin, compat_origin_cap;
/* -C second positional: an already written fix (diff or script) applied to the
 * post-origin+target tree before handover. It lands after the baseline
 * snapshot, so it is part of the derived compat patch. */
static const char *compat_pre;

enum strategy {
	STRAT_ABS,          /* absolute line numbers (;c for single-line diffs) */
	STRAT_REL,          /* f> regex search (s// for single-line diffs) */
};

/* Raw input lines, re-emitted as the === PATCH === tail */
static char **raw_lines;
static int nraw, raw_cap;

/* String vector; a compat block owns its === PATCH === lines this way. */
typedef struct { char **v; int n, cap; } strv_t;
static void arr_append(char ***arr, int *n, int *cap, const char *s);

/* When set, add_raw() appends here instead of raw_lines[], so a compat diff
 * lands in its block's storage and the host === PATCH === stays byte-identical. */
static strv_t *raw_sink;

/* Bytes appearing in the patch content, so SEP/ESC can avoid them */
static unsigned char byte_used[256];

/* Ex escape byte, picked from the bytes the patch does not use (0 = none left,
 * keep the default backslash paths). With it, backslash is not special to
 * ex_arg and content/regex escapes pass through unmodified. */
static int dyn_esc;

/* Ex command separator, likewise. Both go into the script as raw bytes: the
 * body is a single-quoted printf argument, so sh passes all but ' through. */
static int sep;

static void *ecalloc(size_t n, size_t sz)
{
	void *p = calloc(n, sz);
	if (!p) {
		fprintf(stderr, "out of memory\n");
		out_cleanup();
		exit(1);
	}
	return p;
}

static void add_raw(const char *line)
{
	if (raw_sink)
		arr_append(&raw_sink->v, &raw_sink->n, &raw_sink->cap, line);
	else
		arr_append(&raw_lines, &nraw, &raw_cap, line);
}

/* Remove trailing newline, keeping the sbuf length in step so the caller
 * can append to (and cut back) the chomped line. */
static char *chomp_sb(sbuf *sb)
{
	while (sb->s_n && (sb->s[sb->s_n - 1] == '\n' ||
			   sb->s[sb->s_n - 1] == '\r'))
		sb->s_n--;
	sbufn_ret(sb, sb->s)
}

static void chomp(char *s)
{
	int n = strlen(s);
	while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r'))
		s[--n] = '\0';
}

/* One line of any length into sb, newline included; NULL at EOF. sb is reused,
 * so callers may modify it in place and leave the loop without freeing. */
static char *read_line(FILE *f, sbuf *sb)
{
	int c;
	sbuf_cut(sb, 0)
	while ((c = getc(f)) != EOF) {
		sbuf_chr(sb, c)
		if (c == '\n')
			break;
	}
	if (!sb->s_n)
		return NULL;
	sbufn_ret(sb, sb->s)
}

/* Backslash-escape every char of s that appears in set. */
static char *escape_chars(const char *s, const char *set)
{
	sbuf_smake(sb, strlen(s) + 8)
	for (; *s; s++) {
		if (strchr(set, *s))
			sbuf_chr(sb, '\\')
		sbuf_chr(sb, *s)
	}
	sbufn_ret(sb, sb->s)
}

#define REGEX_META "\\^$.*+?[(){|"
static char *escape_regex(const char *s)
{
	return escape_chars(s, REGEX_META);
}

/* Double backslashes for ex_arg level escaping.
 * ex_arg treats \\ as escaped \, so \\\\ is needed to preserve \\.
 * With a dynamic escape byte, backslash is not special to ex_arg and
 * passes through as-is (the escape byte never occurs in content). */
static char *escape_exarg(const char *s)
{
	return escape_chars(s, dyn_esc ? "" : "\\");
}

/* Append s through the ex_arg escape layer: content text, a user-edited regex
 * and an already sub-escaped s/// field all need exactly this and nothing more. */
static void sb_exarg(sbuf *out, const char *s)
{
	char *e = escape_exarg(s);
	sb_str(out, e);
	free(e);
}

/* Append a copy of s to a dynamic array. */
static void arr_append(char ***arr, int *n, int *cap, const char *s)
{
	if (*n >= *cap) {
		*cap = *cap ? *cap * 2 : 4;
		*arr = erealloc(*arr, *cap * sizeof(char *));
	}
	(*arr)[(*n)++] = uc_dup(s);
}

/* A NUL-terminated copy of n bytes of s (byte-exact, where uc_sub() would
 * count characters). */
static char *dup_n(const char *s, int n)
{
	char *r = emalloc(n + 1);
	memcpy(r, s, n);
	r[n] = '\0';
	return r;
}

/* Free n owned strings and the array holding them. */
static void free_lines(char **v, int n)
{
	for (int i = 0; i < n; i++)
		free(v[i]);
	free(v);
}

static int lines_equal(char **a, int na, char **b, int nb)
{
	if (na != nb)
		return 0;
	for (int i = 0; i < na; i++)
		if (strcmp(a[i], b[i]) != 0)
			return 0;
	return 1;
}

/*
 * File-aware anchor validation. The strict-to-loose fallback chain is emitted
 * blind and resolved by nextvi at apply time; when the pre-patch original is
 * readable (it usually is - the script applies in the same tree), candidate
 * anchors are counted against it so only proven-unique ones are added. The
 * chain is emitted either way, so the script stays portable.
 */
static char **orig_lines;   /* pre-patch original, NULL if unreadable */
static int n_orig_lines;

/* A file as a line array, newlines stripped. Missing file: no lines, and
 * *is_new is set, so it is diffed as a creation. */
static char **read_lines(const char *path, int *n, int *is_new)
{
	char **v = NULL;
	int cap = 0;
	FILE *f = fopen(path, "r");
	*n = 0;
	*is_new = !f;
	if (!f)
		return NULL;
	sbuf_smake(lb, SB_INIT)
	while (read_line(f, lb)) {
		if (lb->s[lb->s_n - 1] == '\n')
			sbufn_cut(lb, lb->s_n - 1)
		arr_append(&v, n, &cap, lb->s);
	}
	free(lb->s);
	fclose(f);
	return v;
}

/* A whole file as one string, NULL if it cannot be opened. */
static char *file_text(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	sbuf_smake(sb, SB_INIT)
	sbuf_smake(lb, SB_INIT)
	while (read_line(f, lb))
		sbuf_str(sb, lb->s)
	free(lb->s);
	fclose(f);
	sbufn_ret(sb, sb->s)
}

static void load_orig_file(const char *path)
{
	int is_new;
	orig_lines = read_lines(path, &n_orig_lines, &is_new);
}

static void free_orig_file(void)
{
	for (int i = 0; i < n_orig_lines; i++)
		free(orig_lines[i]);
	free(orig_lines);
	orig_lines = NULL;
	n_orig_lines = 0;
}

/* True if window[0..n) matches orig_lines exactly at 0-based index idx. */
static int window_at(char **window, int n, int idx)
{
	if (idx < 0 || n <= 0 || idx + n > n_orig_lines)
		return 0;
	for (int j = 0; j < n; j++)
		if (strcmp(orig_lines[idx + j], window[j]) != 0)
			return 0;
	return 1;
}

/* Count consecutive-line matches of an n-line window in orig_lines, where
 * line j of a candidate matches when eq(orig line, win, j) holds. Sets
 * *first to the 0-based start of the first match (-1 if none). */
static int count_window_by(const void *win, int n, int *first,
			   int (*eq)(const char *, const void *, int))
{
	int cnt = 0, i, j;
	*first = -1;
	if (!orig_lines || n <= 0 || n > n_orig_lines)
		return 0;
	for (i = 0; i + n <= n_orig_lines; i++) {
		for (j = 0; j < n && eq(orig_lines[i + j], win, j); j++)
			;
		if (j < n)
			continue;
		if (*first < 0)
			*first = i;
		cnt++;
	}
	return cnt;
}

static int m_exact(const char *ln, const void *win, int j)
{
	return !strcmp(ln, ((char **)win)[j]);
}

/* Count exact consecutive-line matches of window[0..n) in orig_lines. */
static int count_window(char **window, int n, int *first)
{
	return count_window_by(window, n, first, m_exact);
}

/* count_window with substring semantics: win[j] must occur inside the aligned
 * original line (empty matches any), as pattern 7's ".*TEXT.*" window does. */
static int m_substr(const char *ln, const void *win, int j)
{
	const char *w = ((char **)win)[j];
	return !w[0] || strstr(ln, w) != NULL;
}

/* Orig lines in [from..to] containing s. Proves a pattern-8 bottom anchor
 * unambiguous: greedy ".*(bottom)" takes the last occurrence, so the chosen
 * line must be the only one carrying that text down to EOF. */
static int count_substr_range(const char *s, int from, int to)
{
	int cnt = 0;
	if (from < 0)
		from = 0;
	if (to > n_orig_lines - 1)
		to = n_orig_lines - 1;
	for (int i = from; i <= to; i++)
		if (s[0] && strstr(orig_lines[i], s))
			cnt++;
	return cnt;
}

/*
 * File-validated fuzzed (relaxed) anchors: an exact anchor with selected runes
 * replaced by '.', kept only if the relaxed form still resolves uniquely to the
 * right place. Length-preserving (one '.' = one rune), so it tolerates in-place
 * drift - a renamed equal-length token, a changed digit - and nothing else.
 */

/* Count runes in the first len bytes of s. */
static int rune_count_n(const char *s, int len)
{
	int n = 0;
	for (int i = 0; i < len; i++)
		if ((s[i] & 0xC0) != 0x80)
			n++;
	return n;
}

/* A fuzzed line: base text plus a per-rune wildcard mask (1 = becomes '.'). */
typedef struct {
	const char *base;     /* borrowed plain text */
	unsigned char *mask;  /* nrune bytes, owned */
	int nrune;
} fline_t;

/* True if orig matches the fuzzed line: same rune count, and every unmasked
 * rune is byte-identical (masked runes match any single rune). */
static int match_fuzzy_line(const char *orig, const fline_t *f)
{
	const char *o = orig, *b = f->base;
	for (int i = 0; i < f->nrune; i++) {
		if (!*o)
			return 0;
		int ol = uc_len(o), bl = uc_len(b);
		if (!f->mask[i] && (ol != bl || memcmp(o, b, ol) != 0))
			return 0;
		o += ol;
		b += bl;
	}
	return *o == 0;
}

/* count_window_by with fuzzed-window semantics: the aligned original line must
 * match win[j]'s literal runes, its masked ones standing for anything. */
static int m_fuzzy(const char *ln, const void *win, int j)
{
	return match_fuzzy_line(ln, &((fline_t *)win)[j]);
}

/* Build the pre-escaped regex for a fuzzed line: masked runes emit '.', literal
 * runes are regex-escaped. */
static char *fuzzy_regex(const fline_t *f)
{
	sbuf_smake(sb, strlen(f->base) + 8)
	const char *b = f->base;
	for (int i = 0; i < f->nrune; i++) {
		int bl = uc_len(b);
		if (f->mask[i]) {
			sbuf_chr(sb, '.')
		} else {
			for (int k = 0; k < bl; k++) {
				if (b[k] && strchr(REGEX_META, b[k]))
					sbuf_chr(sb, '\\')
				sbuf_chr(sb, b[k])
			}
		}
		b += bl;
	}
	sbufn_ret(sb, sb->s)
}

/* Seeded string hash; the fuzz seed and the diff line census share it. */
static unsigned hash_str(const char *s, unsigned h)
{
	while (*s)
		h = h * 131u + (unsigned char)*s++;
	return h;
}

/* Per-position pseudo-value, content-seeded so fuzzing is reproducible. */
static unsigned hash_pos(unsigned seed, int i)
{
	unsigned h = seed ^ 0x9e3779b9u;
	h ^= (unsigned)i * 2654435761u;
	h ^= h >> 13;
	h *= 0x85ebca6bu;
	h ^= h >> 16;
	return h;
}

/* Highest fuzz level tried; level 0 is the lightest relaxation. */
#define FUZZ_MAXLVL 8
#define FUZZ_MASK_MAX 800   /* per-mille: mask up to ~80% of runes (~20% literal) */

/* Fill mask[0..nrune) for fuzz level lvl over a window-global rune index
 * starting at *gi. The drop threshold grows with lvl but never past
 * FUZZ_MASK_MAX; at least one rune always stays literal. */
static void fuzz_mask(unsigned char *mask, int nrune, int lvl, unsigned seed,
		      int *gi)
{
	int thr = (lvl + 1) * FUZZ_MASK_MAX / (FUZZ_MAXLVL + 1);
	int kept = 0;
	for (int i = 0; i < nrune; i++) {
		int g = (*gi)++;
		int drop = (int)(hash_pos(seed, g) % 1000) < thr;
		mask[i] = drop ? 1 : 0;
		if (!drop)
			kept++;
	}
	if (!kept && nrune > 0)
		mask[0] = 0;  /* never wildcard an entire line away */
}

/* Number of (overlapping) occurrences of needle in haystack, both counted. */
static int str_count_occ(const char *hay, int hl, const char *ndl, int nl)
{
	int c = 0;
	if (nl <= 0 || nl > hl)
		return 0;
	for (int i = 0; i + nl <= hl; i++)
		if (memcmp(hay + i, ndl, nl) == 0)
			c++;
	return c;
}

/* Count occurrences of s[start..end) as a substring of s. */
static int range_occurs(const char *s, int start, int end)
{
	return str_count_occ(s, strlen(s), s + start, end - start);
}

/* Step os back 1 byte and over UTF-8 continuation bytes, mirroring on ns. */
static int diff_expand_left(const char *old, int *os, int *ns)
{
	if (*os <= 0)
		return 0;
	int prev = *os;
	(*os)--;
	while (*os > 0 && (old[*os] & 0xC0) == 0x80)
		(*os)--;
	*ns -= (prev - *os);
	return 1;
}

/* Step oe forward 1 byte and over UTF-8 continuation bytes, mirroring on ne. */
static int diff_expand_right(const char *old, int olen, int *oe, int *ne)
{
	if (*oe >= olen)
		return 0;
	int prev = *oe;
	(*oe)++;
	while (*oe < olen && (old[*oe] & 0xC0) == 0x80)
		(*oe)++;
	*ne += (*oe - prev);
	return 1;
}

/* Common byte prefix and suffix of two lines, each snapped back to a rune
 * boundary so runes sharing lead bytes are never split; they never overlap. */
static void common_affix(const char *old, const char *new, int *pre, int *suf)
{
	int old_len = strlen(old), new_len = strlen(new), prefix = 0, suffix = 0;
	while (old[prefix] && new[prefix] && old[prefix] == new[prefix])
		prefix++;
	while (prefix > 0 && (old[prefix] & 0xC0) == 0x80)
		prefix--;
	while (suffix < old_len - prefix && suffix < new_len - prefix &&
	       old[old_len - 1 - suffix] == new[new_len - 1 - suffix])
		suffix++;
	while (suffix > 0 && (old[old_len - suffix] & 0xC0) == 0x80)
		suffix--;
	*pre = prefix;
	*suf = suffix;
}

/*
 * Compare two lines and find the differing portion.
 * Returns 1 if suitable for horizontal edit, 0 otherwise.
 * Sets *old_text to the differing old-side span and *new_text to its
 * replacement (both allocated).
 */
static int find_line_diff(const char *old, const char *new,
			  char **old_text, char **new_text)
{
	int old_len = strlen(old);
	int new_len = strlen(new);
	int prefix, suffix;
	common_affix(old, new, &prefix, &suffix);
	int old_diff_start = prefix;
	int old_diff_end = old_len - suffix;
	int new_diff_start = prefix;
	int new_diff_end = new_len - suffix;

	/* at least half the line common, and less than half of it changing */
	int common = prefix + suffix;
	if (common < old_len / 2 && common < new_len / 2)
		return 0;
	int old_diff_len = old_diff_end - old_diff_start;
	int new_diff_len = new_diff_end - new_diff_start;
	if (old_diff_len > old_len / 2 && new_diff_len > new_len / 2)
		return 0;

	/* Pure insertion: old_text is empty, expand to get searchable context.
	 * Empty search pattern in s// is never valid. */
	if (old_diff_end == old_diff_start) {
		diff_expand_left(old, &old_diff_start, &new_diff_start);
		diff_expand_right(old, old_len, &old_diff_end, &new_diff_end);
		if (old_diff_end == old_diff_start)
			return 0;
	}

	/* Expand diff region until old_text is unique on the line.
	 * Since prefix and suffix are shared between old and new,
	 * expanding symmetrically keeps both regions aligned. */
	while (old_diff_end - old_diff_start > 0) {
		if (range_occurs(old, old_diff_start, old_diff_end) <= 1)
			break;
		/* Prefer left expansion, then right if still not unique. */
		int expanded = diff_expand_left(old, &old_diff_start, &new_diff_start);
		if (!expanded || range_occurs(old, old_diff_start, old_diff_end) > 1)
			expanded |= diff_expand_right(old, old_len, &old_diff_end, &new_diff_end);
		if (!expanded)
			break;
		if (old_diff_start == 0 && old_diff_end == old_len)
			break;
	}

	*old_text = dup_n(old + old_diff_start, old_diff_end - old_diff_start);
	*new_text = dup_n(new + new_diff_start, new_diff_end - new_diff_start);
	return 1;
}

static void mark_bytes_used(const char *s)
{
	for (; *s; s++)
		byte_used[(unsigned char)*s] = 1;
}

/* The lowest unused byte; the low (non-printable) ones come first so printable
 * chars stay available for ex commands and patterns. */
static int find_unused_byte(void)
{
	for (int c = 1; c < 256; c++)
		if (!byte_used[c])
			return c;
	return -1;  /* All bytes used - very unlikely */
}

/* List all unused bytes suitable as separators */
static void list_unused_bytes(sbuf *out)
{
	int n = 0;
	sb_str(out, "# Available separators:");
	int range_start = -1;
	for (int c = 1; c <= 256; c++) {
		int unused = (c < 256) && !byte_used[c];
		if (unused && range_start < 0) {
			range_start = c;
		} else if (!unused && range_start >= 0) {
			int range_end = c - 1;
			if (range_end == range_start)
				sb_printf(out, " 0%03o", range_start);
			else
				sb_printf(out, " 0%03o-0%03o", range_start, range_end);
			n++;
			range_start = -1;
		}
	}
	if (!n)
		sb_str(out, " (none)");
	sb_chr(out, '\n');
}

/* Parse a hunk header: @@ -old_start,old_count +new_start,new_count @@ */
static int parse_hunk_header(const char *line, int *old_start, int *old_count)
{
	if (strncmp(line, "@@ -", 4) != 0)
		return 0;
	const char *p = line + 4;
	*old_start = atoi(p);
	*old_count = 1;
	while (*p && *p != ',' && *p != ' ')
		p++;
	if (*p == ',') {
		p++;
		*old_count = atoi(p);
	}
	/* Verify '+' field exists to confirm it's a valid hunk header */
	while (*p && *p != '+')
		p++;
	return *p == '+';
}

/* One raw byte into a shell double-quoted word: only the four bytes sh still
 * reads there need escaping. */
static void sb_dq(sbuf *out, int c)
{
	if (c == '\\' || c == '$' || c == '`' || c == '"')
		sb_chr(out, '\\');
	sb_chr(out, c);
}

/* A command body as a single-quoted printf argument. A quote is the one byte
 * such a word cannot hold, so each is closed, escaped and reopened; everything
 * else, control bytes and the separator included, goes out verbatim. */
static void sq_write(const char *s, int n)
{
	for (int i = 0; i < n; i++) {
		if (s[i] == '\'')
			fputs("'\\''", stdout);
		else
			putchar((unsigned char)s[i]);
	}
}

/* A file path as a whole single-quoted shell word: an argument of the $VI
 * line, quoted by the same rule sq_write applies inside a printf argument.
 * A path is data, not a word the shell parses, so nothing else in it needs
 * escaping - single quotes are only special where this quoting opens. */
static void sq_path(const char *s)
{
	putchar('\'');
	for (; *s; s++) {
		if (*s == '\'')
			fputs("'\\''", stdout);
		else
			putchar((unsigned char)*s);
	}
	putchar('\'');
}

/* the body register, and the EXINIT that yanks the body buffer into it
 * and runs it; -e fills the register itself and needs neither */
#define P2VI_REG 97
#define P2VI_VICALL "EXINIT='%ya 97:? %@97'"

/*
 * Script state lives in ex registers, not shell variables. A shell variable is
 * a flat string spliced into a site whose nesting depth the shell cannot know;
 * a register is escaped once where it is defined and "?%@<id>" runs it verbatim
 * at any depth (ex_arg sbuf_mem's the expansion, never rescanning it).
 *
 * Definedness is the switch: an undefined register expands to nothing and "?"
 * with an empty argument runs nothing, so the shell only ever contributes whole
 * commands ("${DBG1:+213reg ...}") that define or clear one.
 *
 * Expansion is turned on ("2sc %") for a call and off again ("2sc!") at once,
 * since with xexp live every % in an argument would expand and arguments carry
 * file-derived regexes; argument registers are therefore written before the
 * window, never inside it. xexe (!) stays 0 throughout - a stray ! in a live
 * argument would fork a shell.
 */
#define REG_QF1  210	/* phase-1 quit chain, set by QF1=1 */
#define REG_QF2  211	/* phase-2 quit chain, emptied through REG_QF2A by QF2=1 */
#define REG_INTR 212	/* interrupt chain, set by INTR=1 */
#define REG_ERR1 213	/* phase-1 FAIL chain, set by DBG1=1 */
#define REG_ERR2 214	/* phase-2 FAIL chain, cleared by DBG2=1 */
#define REG_OK1  215	/* phase-1 OK chain, set by DBG1=1 */
#define REG_OK2  216	/* phase-2 OK chain, cleared by DBG2=1 */
#define REG_HDLR 217	/* the FAIL report chain both phases share */
#define REG_LOC  219	/* argument: the FAIL location of the current site */
#define REG_MSG  220	/* argument: the OK report command of the current site */
/* The FAIL log: the report chain redirects its print here ("prF") instead of to
 * the terminal alone, so every failure of a QF2=1 run accumulates in one
 * register the run can be read back from. An upper-case id is the point - that
 * is the range ex_cprint newline-terminates each append in, which makes the
 * register a line per failure and so parseable. */
#define REG_FLOG 'F'
/* The "vis 2; q!1" body every assert runs through, and the one register QF2=1
 * clears. REG_QF2 only ever points at it ("? %@221"), so the -C blocks that
 * rewrite REG_QF2 at run time - the host override and each block's quit policy -
 * rewrite which assert applies, never whether asserting is enabled at all: with
 * QF2=1 in the environment 221 is empty and every one of those rewrites lands
 * on a body that reports and falls through. */
#define REG_QF2A 221
/*
 * -C registers. Registers are global to the process and never cleared between
 * chains, so a value a block's call writes crosses the host body:
 *   REG_APPLIED       the applied set: $P2VI_PATCH verbatim, padded with a
 *                     space at each end, the one thing the shell contributes.
 *                     The driver decides every identity gate out of it with
 *                     "fr <REG_APPLIED>" and a search, so the decision is the
 *                     editor's and the -e path reaches it by setting the same
 *                     variable.
 *   REG_FLAG_ANY      "any origin present", read once by the host override to
 *                     relax its quit chain.
 *   REG_SEC_BASE+k    one per compat block k: the block's own section body,
 *                     yanked there only when every src= of that block is in
 *                     the applied set. Definedness is the switch here too, so
 *                     a block needs neither a flag nor a gate of its own - its
 *                     call is a plain "? %@<reg>", and on a block that does not
 *                     apply that expands to nothing and runs nothing. The same
 *                     number is the ec_while anchor slot the gate records its
 *                     answer in, for an earlier block's quit policy to read.
 * All three sit above the 210-220 control band, and nothing typeable is used
 * at all. The per-src= anchor slots use ec_while ids >= 20, above every
 * single-digit group chain tag, so they never fuse; blocks reusing them is
 * fine, since each records them immediately before reading them and a lookup
 * takes the last record. A gate's answer, which has to outlive the next
 * block's scans, is recorded under the block's own register number instead.
 */
#define REG_APPLIED   229	/* the applied set, i.e. $P2VI_PATCH */
#define REG_FLAG_ANY  230	/* shared any-origin-fired register */
#define REG_SEC_BASE  231	/* per-compat-block section registers: base+k */
#define SRC_SLOT_BASE  20	/* ec_while src= membership anchor slots */

static void emit_esc_sep(sbuf *out, int n)
{
	while (n-- > 0)
		sb_chr(out, dyn_esc ? dyn_esc : '\\');
	sb_chr(out, sep);
}

/* the same in the one part of a body the shell writes: a double-quoted word,
 * where a raw escape or separator byte may need escaping again */
static void sb_dq_esc_sep(sbuf *out, int n)
{
	while (n-- > 0)
		sb_dq(out, dyn_esc ? dyn_esc : '\\');
	sb_dq(out, sep);
}

#define EMIT_SEP(out) emit_esc_sep(out, 0)
/* escaped separator inside ??! block: <esc><sep> for ex_arg */
#define EMIT_ESCSEP(out) emit_esc_sep(out, 1)
/* triply-escaped separator inside a ?? then-arg nested in a ? cond:
 * <esc><esc><esc><sep> */
#define EMIT_ESC3SEP(out) emit_esc_sep(out, 3)
/* Whether the buffer already ends in a line break ("0?" and its newline)
 * followed by nothing but separator/escape decoration. Cuts that decoration
 * back, so the next thing appended supplies its own separator. */
static int lb_pending(sbuf *out)
{
	int esc = dyn_esc ? dyn_esc : '\\';
	int n = out->s_n;
	while (n > 0 && (out->s[n - 1] == sep || out->s[n - 1] == esc))
		n--;
	if (n < 3 || out->s[n - 1] != '\n' || out->s[n - 2] != '?' ||
			out->s[n - 3] != '0')
		return 0;
	/* a break is always emitted after a separator or at the start of a
	 * segment, which is what tells it apart from body text ending in "0?" */
	if (n > 3 && out->s[n - 4] != sep && out->s[n - 4] != esc &&
			out->s[n - 4] != '\n')
		return 0;
	out->s_n = n;
	return 1;
}

/* The no-op command that lets a long chain break across source lines.
 * Callers emit a separator before and after it, and the clause they meant to
 * put between two breaks is sometimes empty (an error check that reports
 * nothing, say), which would leave a bare second break behind. */
static void emit_lb(sbuf *out)
{
	if (!lb_pending(out))
		sb_str(out, "0?\n");
}
#define EMIT_LB(out) emit_lb(out)

/* Append one group segment. Segments start with their own line break,
 * redundant when the segment before ended with one. */
static void sb_seg(sbuf *out, const char *seg)
{
	if (!strncmp(seg, "0?\n", 3) && lb_pending(out))
		seg += 3;
	sb_str(out, seg);
}

/*
 * The ex commands emitted here, and what they do to xrow (every one defaults to
 * the current line when given no range, per ex_region()):
 *   i / c / d      advance xrow past what they inserted or deleted; "0i"
 *                  inserts above line 1
 *   f> / f+ / f-   xrow = matched line, xoff = match position
 *   bare address   xrow = end - 1; this is how +N / -N move without a command
 *   s / p          leave xrow and xoff alone
 *   m              sets a line mark ("+2m 0" marks cursor+2 as <0>); the mark
 *                  auto-adjusts in lbuf_replace() as edits above it shift lines
 *   'N             addresses that mark ("'0c" edits the marked line)
 *   vis, w, q!, sc!, ??!   setup, writes, quits, specials, conditionals
 */

/* The content lines of a c/i command. */
static void emit_content(sbuf *out, char **texts, int ntexts)
{
	for (int i = 0; i < ntexts; i++) {
		sb_exarg(out, texts[i]);
		sb_chr(out, '\n');
	}
}

/* Emit ex commands for inserting text after line N.
 * "Ni" inserts after line N; "0i" inserts above the first line.
 * New files have an empty buffer with no addressable line, so the
 * insert is emitted bare ("i"). */
static void emit_insert_after(sbuf *out, int line, char **texts, int ntexts,
			      int is_new)
{
	if (ntexts == 0)
		return;

	if (is_new)
		sb_str(out, "i ");
	else if (line <= 0)
		sb_str(out, "0i ");
	else
		sb_printf(out, "%di ", line);
	emit_content(out, texts, ntexts);
	EMIT_SEP(out);
}

/* Delete lines N to M inclusive. */
static void emit_delete(sbuf *out, int from, int to)
{
	if (from == to)
		sb_printf(out, "%dd", from);
	else
		sb_printf(out, "%d,%dd", from, to);
	EMIT_SEP(out);
}

/* The ";A[;B]c/d" tail of a character-level edit, after the caller's address
 * prefix. An empty replacement over a non-empty span deletes it. */
static void emit_horiz_span(sbuf *out, int start, int end, const char *new_text)
{
	if (!*new_text && start != end) {
		sb_printf(out, ";%d;%dd", start, end);
	} else {
		if (start == end)
			sb_printf(out, ";%dc ", start);
		else
			sb_printf(out, ";%d;%dc ", start, end);
		sb_exarg(out, new_text);
	}
	EMIT_SEP(out);
}

/* The same at an absolute line (no-op when there is nothing to change). */
static void emit_horizontal_change(sbuf *out, int line, int start, int end,
				   const char *new_text)
{
	if (!*new_text && start == end)
		return;
	sb_printf(out, "%d", line);
	emit_horiz_span(out, start, end, new_text);
}

/* Change lines N to M (an empty replacement deletes them). */
static void emit_change(sbuf *out, int from, int to, char **texts, int ntexts)
{
	if (ntexts == 0) {
		emit_delete(out, from, to);
		return;
	}

	if (from == to)
		sb_printf(out, "%dc ", from);
	else
		sb_printf(out, "%d,%dc ", from, to);
	emit_content(out, texts, ntexts);
	EMIT_SEP(out);
}

/*
 * Relative mode: anchor edits by regex search instead of line number.
 *
 * Phase 1 (resolve). The whole buffer is yanked once into the find register
 * (fr 98) right after the file opens, so every group's search runs against a
 * cache that stays byte-identical to the buffer - nothing is edited in this
 * phase. Each group records its target line in a line mark ("+<off>m <id>") and
 * gets up to NSEARCH fallback patterns, strict to loose, first match wins
 * (emit_fallback_chain). ABS-strategy groups mark their original line number
 * as-is: the buffer is still pristine, so no line-delta correction is needed.
 *
 * Phase 2 (commit). Edits address the marks ('0c, '0d, '0,#+Nc, '0s/../../,
 * '0;A;Bc ...), which auto-adjust as edits above them shift lines, so groups
 * apply forward in patch order. Since every search ran before the first edit, a
 * failed anchor aborts with the file untouched.
 *
 * Every search and every phase-2 edit is followed by a ??! check that reports
 * and quits before corrupting the file.
 */

/* One separator at the caller's nesting depth: deep = 0 for a command
 * inside a ??/??! argument, 1 for one nested a further level in. */
static void emit_sep_lvl(sbuf *out, int deep)
{
	if (deep)
		EMIT_ESC3SEP(out);
	else
		EMIT_ESCSEP(out);
}

/* The expansion window calling chain register <reg>: % on, run the chain, %
 * off. Its argument register must already be written, while % was inert. */
static void emit_reg_call(sbuf *out, int reg, int deep)
{
	sb_str(out, "2sc %");
	emit_sep_lvl(out, deep);
	sb_printf(out, "? %%@%d", reg);
	emit_sep_lvl(out, deep);
	sb_str(out, "2sc!");
}

/* Emit the ??! error check after a command that may fail, with the FAIL
 * location it reports through the shared report chain in REG_LOC: phase 1
 * (search) reports <path>:<line>, phase 2 (edit at a mark) adds :m<id>, and
 * mark_id < 0 means no mark (new-file insert, custom abs command).
 * phase selects the report register, whose definedness is the DBG<n> switch
 * and whose chain ends in the phase's INTR and QF<n> calls.
 * ids[0..nids) are the capture tags of a fallback chain - every pattern
 * variant in phase 1, every substitute rung in phase 2 - ORed into one DNF
 * expression prefixing the conditional, so it branches on those recorded
 * statuses instead of the last command's and the inverted branch fires only
 * if all of them failed. */
static void emit_err_check(sbuf *out, int phase, int line, int mark_id,
			   const int *ids, int nids)
{
	sbuf_smake(loc, SB_INIT)
	sb_printf(loc, "%s:%d", cur_file_path ? cur_file_path :
		  phase == 1 ? "?" : "", line);
	if (phase == 2) {
		sb_str(loc, ":m");
		if (mark_id >= 0)
			sb_printf(loc, "%d", mark_id);
	}
	sbuf_nul(loc)
	for (int t = 0; t < nids; t++)
		sb_printf(out, t ? ";%d" : "%d", ids[t]);
	/* "?" "?!" split: "??!" in one literal is the trigraph for '|' */
	sb_printf(out, "?" "?!%dreg %s", REG_LOC, loc->s);
	EMIT_ESCSEP(out);
	emit_reg_call(out, phase == 1 ? REG_ERR1 : REG_ERR2, 0);
	EMIT_SEP(out);
	free(loc->s);
}


/* Everything a search of the given mode needs before its f> argument, plus
 * the verb itself. lvl is the caller's separator nesting depth, first
 * selects f> over f+; a global mode-3 window always forces f>, since it
 * restarts from the reset top. */
static void emit_search_setup(sbuf *out, int mode, int first, int lvl)
{
	int g3 = mode == 3;
	if (g3) {
		sb_printf(out, "m %d", WIN_SAVE_MARK);
		emit_esc_sep(out, lvl);
		sb_str(out, "1;0");
		emit_esc_sep(out, lvl);
	}
	if (mode == 2 || g3) {
		sb_str(out, "grp 1");
		emit_esc_sep(out, lvl);
	}
	if (mode == 1) {
		sb_str(out, ";0");
		emit_esc_sep(out, lvl);
		sb_str(out, "fr");
		emit_esc_sep(out, lvl);
		sb_str(out, first ? ".,$f> " : ".,$f+ ");
	} else
		sb_str(out, (g3 || first) ? "%f> " : "%f+ ");
}

/* The f> argument of a phase-1 search: the pattern's lines joined by newlines.
 * A window generator hands them over pre-escaped and self-anchoring, so they go
 * out as they are; raw text is regex-escaped first, and a lone raw line is
 * wrapped ^...$ so repeated text cannot match at an offset.
 *
 * lvl is how many ex_arg layers the argument sits under - one for a top-level
 * search, two inside a ? conditional - and each of them doubles every
 * backslash. With a dynamic escape byte backslash is not special to ex_arg at
 * all, so no layer needs anything (escape_exarg is the identity there). */
static void sb_pat_lines(sbuf *out, char **lines, int nlines, int pre_escaped,
			 int lvl)
{
	int wrap = nlines == 1 && !pre_escaped;
	if (wrap)
		sb_chr(out, '^');
	for (int i = 0; i < nlines; i++) {
		char *s = pre_escaped ? uc_dup(lines[i])
				      : escape_regex(lines[i]);
		for (int k = dyn_esc ? 0 : lvl; k-- > 0; ) {
			char *e = escape_chars(s, "\\");
			free(s);
			s = e;
		}
		sb_str(out, s);
		free(s);
		if (i < nlines - 1)
			sb_chr(out, '\n');
	}
	if (wrap)
		sb_chr(out, '$');
	/* Ensure trailing newline when the last line is empty */
	if (nlines > 0 && !lines[nlines - 1][0])
		sb_chr(out, '\n');
}

/* The lone-pattern phase-1 search: setup and f>, the pattern (sb_pat_lines,
 * at the top level's one escape layer), the error check, then "+<offset>m
 * <mark_id>" to mark the target without moving the cursor. */
static void emit_search(sbuf *out, char **anchors, int nanchors,
			int offset, int mark_id,
			int target_line, int pre_escaped, int first, int mode)
{
	int single = mode == 1;
	int g3 = mode == 3;
	int grp = mode == 2 || g3;
	emit_search_setup(out, mode, first, 0);
	sb_pat_lines(out, anchors, nanchors, pre_escaped, 1);
	EMIT_SEP(out);
	emit_err_check(out, 1, target_line, -1, NULL, 0);
	if (grp) {
		/* reset the search group. Must come AFTER the error check:
		 * grp 0 succeeds and would otherwise overwrite xpret, masking
		 * a failed f> search from the ??! check above. */
		sb_str(out, "grp 0");
		EMIT_SEP(out);
	}
	if (single) {
		sb_str(out, "fr 98");
		EMIT_SEP(out);
	}
	EMIT_LB(out);
	EMIT_SEP(out);
	if (offset)
		sb_printf(out, "%+d", offset);
	sb_printf(out, "m %d", mark_id);
	EMIT_SEP(out);
	if (g3) {
		/* restore the cursor saved before the global search so the next
		 * group's incremental search continues from the same position */
		sb_printf(out, "'%d", WIN_SAVE_MARK);
		EMIT_SEP(out);
	}
}

/* The next free line mark id, skipping the ids the editor rewrites itself:
 * <'> <*> <[> <]> <`>. */
static int next_mark_id(int *n)
{
	while (*n == '\'' || *n == '*' || *n == '[' || *n == ']' || *n == '`')
		(*n)++;
	return (*n)++;
}

typedef struct group_s {
	int del_start, del_end;  /* 0 if no deletes */
	char **add_texts;
	char **del_texts;        /* deleted line contents */
	int ndel;
	int nadd;
	int add_after;  /* line to add after (for pure adds) */
	/* For relative mode: */
	int anchor_offset;       /* lines from anchor to first change */
	char *anchors[3];        /* up to 3 consecutive preceding context lines */
	int nanchors;            /* count of anchor lines */
	char *follow_ctx;        /* first following context line */
	int follow_offset;       /* lines from first change to follow_ctx */
	char **post_ctx;         /* post-change context lines (up to 3) */
	int npost_ctx;
	int has_line_diff;       /* whether find_line_diff() succeeded */
	char *ld_old_text;       /* expanded diff text for s// */
	char *ld_new_text;       /* expanded replacement text for s// */
	int ldc_start, ldc_end; /* minimal char positions for ;c */
	char *ldc_new_text;      /* minimal replacement text for ;c */
	/* Enclosing @@ hunk's original-line span (1-based, 0 if unknown); used by
	 * gen_win_window to anchor strictly outside the diff's shown region. */
	int hunk_lo, hunk_hi;
	/* Two-phase emission state, set in phase 1, read in phase 2 */
	int res_strat;           /* resolved strategy */
	int mark_id;             /* line mark id, -1 = no mark */
	int insert_i;            /* pure add: insert before mark ('N-1i) vs after ('Ni) */
	/* The generated phase-1/phase-2 segment bytes (gen_group_segments),
	 * written out at emit time; no trailing newline. */
	char *ph1_gen, *ph2_gen;
} group_t;

/* Any text a search could anchor on - leading context, a following context
 * line, a non-empty deleted line? Decides REL vs ABS. */
static int group_has_anchors(group_t *g)
{
	return g->nanchors >= 2
	       || (g->nanchors == 1 && g->anchors[0] && g->anchors[0][0])
	       || (g->follow_ctx && g->follow_ctx[0])
	       || (g->ndel > 0 && g->del_texts[0] && g->del_texts[0][0]);
}


/* One fallback search pattern (phase 1) */
typedef struct {
	char **lines;
	int nlines;
	int pre_escaped;  /* 1 = a window generator's regex, 0 = raw text */
	int offset;       /* lines from match start to the target line */
	int off_final;    /* 1 = the window generator's own offset, which the
			   * pure-add shift must leave alone */
	int mode;         /* search mode: 1 single line, 0 multi-line; windows add 2 and 3 */
	int pid;          /* fixed pattern id (source slot + 1, 1-9): emitted as
			   * the capture tag and OK1 anchor id so a failure maps
			   * to its real pattern regardless of which slots survived */
} pat_spec_t;

/* Default (non-edited) lines for fallback pattern pi, strict to loose:
 *   0 = whole hunk (pre-ctx + deleted + following ctx)
 *   1 = deleted + following ctx: pre-context ambiguous, trailing not
 *   2 = deleted lines only
 *   3 = top context only (the historical single pattern)
 *   4 = following ctx only: the whole hunk region is volatile but the line
 *       after it is a stable landmark
 * The deletion-rooted slots (1, 2) outrank the context-rooted ones (3, 4): they
 * land on the text the patch expects to remove (off = 0), while a context match
 * trusts that nothing drifted between the anchor and the hunk.
 * 1 and 4 need deleted lines and return 0 for a pure add; redundant slots (no
 * following context makes 1 == 2, 4 empty) are dropped by the caller's dedup.
 * raw[] takes borrowed pointers; *off = lines from match start to target. */
static int default_pat_lines(group_t *g, int pi, char **raw, int *off)
{
	int n = 0;
	int has_del = g->ndel > 0 && !(g->ndel == 1 && !g->del_texts[0][0]);
	int has_post = g->npost_ctx > 0 || (g->follow_ctx && g->follow_ctx[0]);
	*off = 0;
	if (pi == 2) {
		if (!has_del)
			return 0;
		for (int i = 0; i < g->ndel; i++)
			raw[n++] = g->del_texts[i];
		return n;
	}
	if (pi == 1) {
		/* deleted lines + following ctx; match starts on the first
		 * deleted line, which is the target (off = 0). Only distinct
		 * from strategy 2 when following context exists. */
		if (!has_del || !has_post)
			return 0;
		for (int i = 0; i < g->ndel; i++)
			raw[n++] = g->del_texts[i];
		if (g->npost_ctx > 0)
			for (int i = 0; i < g->npost_ctx; i++)
				raw[n++] = g->post_ctx[i];
		else
			raw[n++] = g->follow_ctx;
		return n;
	}
	if (pi == 4) {
		/* following ctx only; the post context sits g->ndel lines
		 * below the first deleted line (the target), since the
		 * search-time buffer holds the pre-edit content. */
		if (!has_del || !has_post)
			return 0;
		if (g->npost_ctx > 0)
			for (int i = 0; i < g->npost_ctx; i++)
				raw[n++] = g->post_ctx[i];
		else
			raw[n++] = g->follow_ctx;
		*off = -(g->ndel);
		return n;
	}
	if (pi == 3) {
		if (g->nanchors >= 2 ||
		    (g->nanchors == 1 && g->anchors[0] && g->anchors[0][0])) {
			for (int i = 0; i < g->nanchors; i++)
				raw[n++] = g->anchors[i];
			*off = g->nanchors - 1 + g->anchor_offset;
		} else if (g->follow_ctx && g->follow_ctx[0]) {
			raw[n++] = g->follow_ctx;
			*off = -(g->follow_offset);
		} else if (g->ndel > 0 && g->del_texts[0][0]) {
			raw[n++] = g->del_texts[0];
		}
		return n;
	}
	/* pi == 0: whole hunk */
	for (int i = 0; i < g->nanchors; i++)
		raw[n++] = g->anchors[i];
	int top = n;
	for (int i = 0; i < g->ndel; i++)
		raw[n++] = g->del_texts[i];
	if (g->npost_ctx > 0) {
		for (int i = 0; i < g->npost_ctx; i++)
			raw[n++] = g->post_ctx[i];
	} else if (g->follow_ctx) {
		raw[n++] = g->follow_ctx;
	}
	if (top)
		*off = g->nanchors - 1 + g->anchor_offset;
	else if (!g->ndel && n)
		*off = -(g->follow_offset);
	return n;
}

/* A file-validated relaxed window: pre-escaped regex lines plus the offset and
 * mode needed to emit it like an exact pattern. */
typedef struct {
	char **lines;   /* owned: nlines malloc'd regex strings */
	int nlines;
	int offset;     /* lines from match start to the target line */
	int mode;       /* search mode: 1 single line, 0 multi-line; windows add 2 and 3 */
} fuzzwin_t;

/* Append file-validated window w to ps[nps] with pid; off_final preserves its
 * offset through the pure-add shift. Returns the new nps. */
static int push_win_pat(pat_spec_t *ps, int nps, fuzzwin_t *w, int pid,
			int off_final)
{
	ps[nps].lines = w->lines;
	ps[nps].nlines = w->nlines;
	ps[nps].pre_escaped = 1;
	ps[nps].offset = w->offset;
	ps[nps].off_final = off_final;
	ps[nps].mode = w->mode;
	ps[nps].pid = pid;
	return nps + 1;
}

/*
 * Up to max fuzzed windows for group g into out[]: the whole-hunk window
 * relaxed at increasing levels, keeping each variant the original file proves
 * still resolves to exactly one place - the right one. Needs orig_lines loaded
 * and the hunk pristine. Caller owns out[i].lines.
 */
static int gen_fuzz_windows(group_t *g, fuzzwin_t *out, int max)
{
	if (!orig_lines || max <= 0 || g->del_start <= 0 ||
	    !window_at(g->del_texts, g->ndel, g->del_start - 1))
		return 0;
	char **base = emalloc((g->ndel + 7) * sizeof(char *));
	int doff0;
	int bn = default_pat_lines(g, 0, base, &doff0);
	if (bn <= 0) {
		free(base);
		return 0;
	}
	int expected = (g->del_start - 1) - doff0;
	unsigned seed = 0;
	for (int i = 0; i < bn; i++)
		seed = hash_str(base[i], seed);
	fline_t *win = emalloc(bn * sizeof(*win));
	/* Collect every distinct file-validated variant, strictest (low fuzz
	 * level) first, then keep only the last `max` - the loosest ones. The
	 * level span is fixed (independent of max) so reducing how many we keep
	 * never narrows how loose we are willing to relax. */
	fuzzwin_t cand[FUZZ_MAXLVL + 1];
	int nc = 0;
	for (int lvl = 0; lvl <= FUZZ_MAXLVL; lvl++) {
		int any = 0, gi = 0, masked = 0, total = 0;
		for (int j = 0; j < bn; j++) {
			int nr = uc_slen(base[j]);
			unsigned char *m = emalloc(nr ? nr : 1);
			fuzz_mask(m, nr, lvl, seed, &gi);
			for (int k = 0; k < nr; k++)
				if (m[k])
					any = 1, masked++;
			total += nr;
			win[j].base = base[j];
			win[j].mask = m;
			win[j].nrune = nr;
		}
		/* keep ~20% of runes literal: a window relaxed past four in
		 * five is too thin to trust, however it validates here */
		int too_loose = total > 0 && masked * 5 > total * 4;
		int first, cnt = any && !too_loose
				 ? count_window_by(win, bn, &first, m_fuzzy) : 0;
		if (any && !too_loose && cnt == 1 && first == expected) {
			char **lines = emalloc(bn * sizeof(char *));
			for (int j = 0; j < bn; j++)
				lines[j] = fuzzy_regex(&win[j]);
			int dup = 0;
			for (int p = 0; p < nc; p++)
				if (lines_equal(lines, bn, cand[p].lines,
						cand[p].nlines)) {
					dup = 1;
					break;
				}
			if (dup) {
				for (int j = 0; j < bn; j++)
					free(lines[j]);
				free(lines);
			} else {
				cand[nc].lines = lines;
				cand[nc].nlines = bn;
				cand[nc].offset = doff0;
				cand[nc].mode = bn == 1 ? 1 : 0;
				nc++;
			}
		}
		for (int j = 0; j < bn; j++)
			free(win[j].mask);
	}
	free(win);
	free(base);
	/* Keep the last `max` (loosest); free the stricter ones we drop. */
	int keep = nc < max ? nc : max;
	int drop = nc - keep;
	for (int i = 0; i < drop; i++)
		free_lines(cand[i].lines, cand[i].nlines);
	for (int i = 0; i < keep; i++)
		out[i] = cand[drop + i];
	return keep;
}

static void free_fuzz_windows(fuzzwin_t *w, int n)
{
	for (int i = 0; i < n; i++) {
		free_lines(w[i].lines, w[i].nlines);
		w[i].lines = NULL;
		w[i].nlines = 0;
	}
}

/*
 * Pattern 7: a :grp-capture window (mode 2). The top of the hunk - preceding
 * context plus the first deleted line on change/delete - becomes "TEXT.*?" line
 * by line, the final line captured "(TEXT)". ":grp 1" lands on that captured
 * line, and the non-greedy ".*?" on the leading ones absorbs text added after
 * an anchor without shifting the target. Unanchored, so no leading ".*".
 *
 * The captured last line IS the target at offset 0: change/delete captures the
 * first deleted line, a pure insert the last anchor ("'Ni" appends after it).
 * Voided when degenerate - under two lines (a bare "(text)" duplicates the
 * exact single-line strategies) or an empty capture (zero-width "()" resolves
 * anywhere) - and file-validated like the fuzzed windows.
 */
static int gen_grp_window(group_t *g, fuzzwin_t *out)
{
	if (!orig_lines || g->nanchors < 1 || !g->anchors[g->nanchors - 1])
		return 0;
	int has_del = g->ndel > 0 && !(g->ndel == 1 && !g->del_texts[0][0]);
	int n = g->nanchors + (has_del ? 1 : 0);
	char **raw = emalloc(n * sizeof(char *));
	for (int i = 0; i < g->nanchors; i++)
		raw[i] = g->anchors[i];
	if (has_del)
		raw[g->nanchors] = g->del_texts[0];
	/* The grp window only earns its slot when it has at least one leading
	 * ".*?" anchor to absorb interior drift; a bare "(text)" is just a
	 * redundant single-line search the exact strategies already cover. An
	 * empty captured last line would emit "()" - a zero-width grp match that
	 * resolves anywhere - so reject that too. */
	if (n < 2 || !raw[n - 1][0]) {
		free(raw);
		return 0;
	}
	/* The captured last line must land on the target: change/delete -> the
	 * first deleted line at del_start-1; pure insert -> the last anchor at
	 * add_after-1. The window starts n-1 lines above it. */
	int last = has_del ? g->del_start - 1 : g->add_after - 1;
	int first, cnt;
	if (last < 0 || last - (n - 1) < 0) {
		free(raw);
		return 0;
	}
	cnt = count_window_by(raw, n, &first, m_substr);
	if (cnt != 1 || first != last - (n - 1)) {
		free(raw);
		return 0;
	}
	char **lines = emalloc(n * sizeof(char *));
	for (int i = 0; i < n; i++) {
		char *e = escape_regex(raw[i]);
		int cap = i == n - 1;
		/* The search is unanchored, so a leading ".*" is redundant; each
		 * non-final line takes a trailing non-greedy ".*?" to absorb text
		 * added after the anchor without over-consuming, and the captured
		 * last line needs nothing extra. */
		int len = strlen(e) + 5;   /* "(" + ")" or ".*?", + NUL */
		char *s = emalloc(len);
		snprintf(s, len, cap ? "(%s)" : "%s.*?", e);
		lines[i] = s;
		free(e);
	}
	free(raw);
	out->lines = lines;
	out->nlines = n;
	/* The mark sits on the captured last line (offset 0) in both shapes:
	 * change/delete edits at del_start, and a pure insert's phase-2 "'Ni"
	 * already appends after the marked last anchor, so no +1 is needed. */
	out->offset = 0;
	out->mode = 2;   /* grp register search */
	return 1;
}

/* How far above/below a hunk gen_win_window looks for a unique anchor block;
 * bounds its O(scan * file) validation cost. */
#define WIN_SCAN 200

/* Lines per straddle anchor block: a 3-line block is far more discriminating
 * than a single line, so the global search false-matches less. */
#define WIN_ANCHOR 3

/* True if orig_lines[s .. s+WIN_ANCHOR) are all in-range and non-empty: the
 * precondition for using that block as a straddle anchor. */
static int anchor_block_at(int s)
{
	if (s < 0 || s + WIN_ANCHOR > n_orig_lines)
		return 0;
	for (int j = 0; j < WIN_ANCHOR; j++)
		if (!orig_lines[s + j][0])
			return 0;
	return 1;
}

/*
 * Pattern 8: global "top.*(bottom)" straddle window (mode 3). Both anchors lie
 * OUTSIDE the diff's shown region - the nearest unique WIN_ANCHOR-line block
 * above the enclosing @@ hunk and below it - so these lines exist only in the
 * original and this needs it readable. Regex "t1\nt2\nt3.*(b1)\nb2\nb3": each
 * block newline-joined (consecutive-line match), one greedy ".*" between them
 * absorbing the whole hunk. Only b1 is captured, so ":grp 1" lands on it and the
 * target is a negative offset back up. Emitted bracketed by mark-0 save / "1;0"
 * reset / "'0" restore, so the search runs from the file top.
 *
 * skip=1 is pattern 9: skip the first qualifying block on each side, advancing a
 * WHOLE block so the two windows stay disjoint - a farther, looser straddle last
 * in the chain.
 *
 * File-validated: top block unique; bottom block unique AND its captured first
 * line carries its text nowhere down to EOF, so the greedy ".*" lands on
 * exactly it.
 */
static int gen_win_window(group_t *g, fuzzwin_t *out, int skip)
{
	if (!orig_lines)
		return 0;
	/* 0-based line the mark must land on (the target). For a change/delete it is
	 * the first deleted line; for a pure insert it is the existing line the new
	 * text is appended after. Pristine: the deleted lines must still be present
	 * (change/delete), or the added lines must NOT yet be present and the
	 * insertion boundary must match the original (pure insert) - else the file is
	 * not the pre-patch original and the offset would be wrong. */
	int hunk_top;
	if (g->del_start > 0) {
		if (!window_at(g->del_texts, g->ndel, g->del_start - 1))
			return 0;
		hunk_top = g->del_start - 1;
	} else {
		if (g->nadd <= 0 || g->add_after < 1 || g->add_after > n_orig_lines ||
		    g->nanchors < 1 ||
		    strcmp(orig_lines[g->add_after - 1], g->anchors[g->nanchors - 1]) != 0 ||
		    window_at(g->add_texts, g->nadd, g->add_after))
			return 0;
		hunk_top = g->add_after - 1;
	}
	/* The anchors must lie OUTSIDE the diff's shown region, not on its context
	 * lines (those are exactly what may drift and what the other strategies
	 * already key on). Skip past the whole enclosing @@ hunk - including all its
	 * shown context - so top/bottom come only from the original file beyond it.
	 * Fall back to the deleted range if the span is unknown. */
	int span_lo = g->hunk_lo > 0 ? g->hunk_lo - 1 : hunk_top;
	int span_hi = g->hunk_hi > 0 ? g->hunk_hi - 1
		      : g->del_end > 0 ? g->del_end - 1
		      : hunk_top;
	if (span_lo > hunk_top)
		span_lo = hunk_top;
	if (span_hi < hunk_top)
		span_hi = hunk_top;
	/* nearest unique non-empty WIN_ANCHOR-line block ending strictly above the
	 * hunk's shown region; skip past the first `skip` qualifying blocks for a
	 * farther anchor. `it` is the block start (top line). When skipping, advance a
	 * WHOLE block (s -= WIN_ANCHOR - 1, plus the loop's own s--) so pattern 9's
	 * block does not overlap pattern 8's - they must be disjoint, not shifted by
	 * one line. */
	int it = -1, first, seen = 0;
	for (int s = span_lo - WIN_ANCHOR, d = 0; s >= 0 && d < WIN_SCAN; s--, d++) {
		if (!anchor_block_at(s))
			continue;
		if (count_window(&orig_lines[s], WIN_ANCHOR, &first) == 1) {
			if (seen++ < skip) {
				s -= WIN_ANCHOR - 1;
				continue;
			}
			it = s;
			break;
		}
	}
	/* nearest unique non-empty WIN_ANCHOR-line block starting strictly below the
	 * hunk's shown region, with its captured first line unambiguous as a substring
	 * from that line to EOF (so greedy ".*" lands on it); skip past the first
	 * `skip` qualifying blocks for a farther anchor (advancing a whole block so the
	 * windows stay disjoint). `ib` is the captured line. */
	int ib = -1;
	seen = 0;
	for (int s = span_hi + 1, d = 0; s + WIN_ANCHOR <= n_orig_lines && d < WIN_SCAN;
	     s++, d++) {
		if (!anchor_block_at(s))
			continue;
		if (count_window(&orig_lines[s], WIN_ANCHOR, &first) == 1 &&
		    count_substr_range(orig_lines[s], s + 1, n_orig_lines - 1) == 0) {
			if (seen++ < skip) {
				s += WIN_ANCHOR - 1;
				continue;
			}
			ib = s;
			break;
		}
	}
	if (it < 0 || ib < 0)
		return 0;
	/* Build "t1\nt2\nt3.*(b1)\nb2\nb3": each block's lines are joined with a
	 * literal newline (consecutive-line match, in multi-line search mode), so the
	 * blocks only STRENGTHEN the anchoring - there is exactly ONE ".*", the single
	 * gap that absorbs the hunk between the two blocks. Only the first bottom line
	 * is captured "(b1)" so grp lands on it and the offset reference stays at ib. */
	char *e;
	sbuf_smake(sb, 256)
	for (int j = 0; j < WIN_ANCHOR; j++) {           /* t1\nt2\nt3 */
		e = escape_regex(orig_lines[it + j]);
		if (j)
			sbuf_chr(sb, '\n')
		sbuf_str(sb, e)
		free(e);
	}
	for (int j = 0; j < WIN_ANCHOR; j++) {           /* .*(b1)\nb2\nb3 */
		e = escape_regex(orig_lines[ib + j]);
		if (j) {
			sbuf_chr(sb, '\n')
			sbuf_str(sb, e)
		} else {
			sbuf_str(sb, ".*(")               /* one ".*", capture b1 */
			sbuf_str(sb, e)
			sbuf_chr(sb, ')')
		}
		free(e);
	}
	sbuf_nul4(sb)
	char **lines = emalloc(sizeof(char *));
	lines[0] = sb->s;
	out->lines = lines;
	out->nlines = 1;
	out->offset = hunk_top - ib;   /* negative: target sits above the bottom anchor */
	out->mode = 3;                 /* global grp straddle window */
	return 1;
}

/* Every file-validated relaxed window of one group, slot i holding pattern slot
 * NPAT + i: the fuzz slots first, then GRP_SLOT, WIN_SLOT and WIN2_SLOT. */
typedef struct {
	fuzzwin_t w[NSEARCH - NPAT];
	int has[NSEARCH - NPAT];
} winset_t;

static void gen_extra_windows(group_t *g, winset_t *ws)
{
	memset(ws, 0, sizeof(*ws));
	for (int i = gen_fuzz_windows(g, ws->w, NFUZZ); i-- > 0; )
		ws->has[i] = 1;
	ws->has[GRP_SLOT - NPAT] = gen_grp_window(g, &ws->w[GRP_SLOT - NPAT]);
	ws->has[WIN_SLOT - NPAT] = gen_win_window(g, &ws->w[WIN_SLOT - NPAT], 0);
	ws->has[WIN2_SLOT - NPAT] = gen_win_window(g, &ws->w[WIN2_SLOT - NPAT], 1);
}

static void free_extra_windows(winset_t *ws)
{
	for (int i = 0; i < NSEARCH - NPAT; i++)
		if (ws->has[i])
			free_fuzz_windows(&ws->w[i], 1);
}

/* Phase 1 fallback chain: every pattern nested into one ? conditional, chained
 * with escaped separators, first match wins. Per pattern n (capture tag n):
 *   %f> <pat>\:<n>??\:<n>??[+off]m <id>\\\:<220>reg p OK <loc>:a<n>\\\:%@215:1q\:
 * (the OK report only on fallback blocks, n >= 1). The search's status is
 * captured into tag <n>; on success that branch marks the target and 1q
 * short-circuits out. After the last block a single <0;1;..>??! DNF check over
 * all tags reports the failure.
 * Each attempt carries its mode's setup and the teardown it implies: mode 1
 * restores the register cache ("fr 98") on both paths, modes 2 and 3 reset the
 * search group, mode 3 restores the saved cursor before the conditional 1q. */
static void emit_fallback_chain(sbuf *out, pat_spec_t *ps, int nps,
				int mark_id, int target_line, int first)
{
	int pids[NSEARCH];
	sb_chr(out, '?');
	for (int n = 0; n < nps; n++) {
		int m1 = ps[n].mode == 1;
		int g3 = ps[n].mode == 3;
		int g2 = ps[n].mode == 2 || g3;   /* grp bracketing covers both */
		/* Readability line break, so every attempt starts on its own
		 * source line: separator, no-op clause, newline, then the
		 * separator before the search setup. */
		EMIT_ESCSEP(out);
		EMIT_LB(out);
		EMIT_ESCSEP(out);
		/* the mode's own setup, one escape level deeper than a
		 * top-level search; a mode-1 attempt's "fr 98" below puts the
		 * register cache back for the attempts after it */
		emit_search_setup(out, ps[n].mode, first, 1);
		/* one ex_arg layer deeper than a top-level search: the whole
		 * f> sits inside the ? conditional's argument */
		sb_pat_lines(out, ps[n].lines, ps[n].nlines,
			     ps[n].pre_escaped, 2);
		EMIT_ESCSEP(out);
		sb_printf(out, "%d??", ps[n].pid);
		/* The same once the result is captured into tag <n>, splitting
		 * the match from its mark action. After the capture (and before
		 * grp 0 / the action re-test), so it never separates a tag test
		 * from its then-arm. */
		EMIT_ESCSEP(out);
		EMIT_LB(out);
		if (g2) {
			/* reset the search group on both match and no-match
			 * paths. Must come AFTER the <n>?? tag capture above,
			 * otherwise the tag records grp 0's (always-success)
			 * status instead of the f> search's. */
			EMIT_ESCSEP(out);
			sb_str(out, "grp 0");
		}
		EMIT_ESCSEP(out);
		sb_printf(out, "%d??", ps[n].pid);
		if (ps[n].offset)
			sb_printf(out, "%+d", ps[n].offset);
		sb_printf(out, "m %d", mark_id);
		/* fallback (non-primary) match: with DBG1=1 the OK chain is
		 * defined and reports which anchor resolved the group */
		if (n) {
			EMIT_ESC3SEP(out);
			sb_printf(out, "%dreg p OK %s:%d:a%d", REG_MSG,
				  cur_file_path ? cur_file_path : "?",
				  target_line, ps[n].pid);
			EMIT_ESC3SEP(out);
			emit_reg_call(out, REG_OK1, 1);
		}
		if (m1) {
			/* restore the register cache on the success path,
			 * before 1q quits out of the chain */
			EMIT_ESC3SEP(out);
			sb_str(out, "fr 98");
		}
		if (g3) {
			/* restore the saved cursor unconditionally (both match
			 * and no-match), at chain level so it runs before any
			 * short-circuit; this undoes the "1;0" reset above */
			EMIT_ESCSEP(out);
			sb_printf(out, "'%d", WIN_SAVE_MARK);
		}
		if (n < nps - 1) {
			if (g3) {
				/* the unconditional restore split the then-arg,
				 * so re-test the tag to keep 1q on success */
				EMIT_ESCSEP(out);
				sb_printf(out, "%d??", ps[n].pid);
				EMIT_ESC3SEP(out);
				sb_str(out, "1q");
			} else {
				/* 1q sits inside the <n>?? then-arg, one level
				 * deeper, so its separator needs three escapes */
				EMIT_ESC3SEP(out);
				sb_str(out, "1q");
			}
		}
		if (m1) {
			/* restore the cache on the no-match fall-through */
			EMIT_ESCSEP(out);
			sb_str(out, "fr 98");
		}
	}
	EMIT_SEP(out);
	EMIT_LB(out);
	EMIT_SEP(out);
	for (int n = 0; n < nps; n++)
		pids[n] = ps[n].pid;
	emit_err_check(out, 1, target_line, -1, pids, nps);
	EMIT_LB(out);
	EMIT_SEP(out);
}

/* Phase 2: delete at a mark */
static void emit_mark_delete(sbuf *out, int line, int mark_id, int count)
{
	if (count == 1)
		sb_printf(out, "'%dd", mark_id);
	else
		sb_printf(out, "'%d,#+%dd", mark_id, count - 1);
	EMIT_SEP(out);
	emit_err_check(out, 2, line, mark_id, NULL, 0);
}

/* Phase 2: insert at a mark ("'Ni" after the mark, "'N-1i" before it).
 * mark_id < 0 means a new file's empty buffer: no line to mark,
 * so the insert is emitted bare. */
static void emit_mark_insert(sbuf *out, int line, int mark_id, int use_i,
			     char **texts, int ntexts)
{
	if (ntexts == 0)
		return;
	if (mark_id < 0)
		sb_str(out, "i ");
	else if (use_i)
		sb_printf(out, "'%d-1i ", mark_id);
	else
		sb_printf(out, "'%di ", mark_id);
	emit_content(out, texts, ntexts);
	EMIT_SEP(out);
	emit_err_check(out, 2, line, mark_id, NULL, 0);
}

/* Phase 2: change lines at a mark */
static void emit_mark_change(sbuf *out, int line, int mark_id,
			     int del_count, char **texts, int ntexts)
{
	if (ntexts == 0) {
		emit_mark_delete(out, line, mark_id, del_count);
		return;
	}
	if (del_count == 1)
		sb_printf(out, "'%dc ", mark_id);
	else
		sb_printf(out, "'%d,#+%dc ", mark_id, del_count - 1);
	emit_content(out, texts, ntexts);
	EMIT_SEP(out);
	emit_err_check(out, 2, line, mark_id, NULL, 0);
}

/* ex_re_read halves a run of escapes sitting against the closing delimiter
 * (ceil(n/2), commit d94cd92). Our escapers already doubled each literal
 * backslash, so double the trailing run once more for ex_re_read to halve back.
 * Only the run adjacent to the delimiter is affected - an interior escape is
 * followed by an ordinary char and passes through unchanged. */
static char *double_trailing_esc(char *s)
{
	int len = strlen(s), t = 0;
	while (t < len && s[len - 1 - t] == '\\')
		t++;
	if (!t)
		return s;
	sbuf_smake(sb, len + t + 1)
	sbuf_mem(sb, s, len)
	sbuf_set(sb, '\\', t)
	free(s);
	sbufn_ret(sb, sb->s)
}

/* s/// replacement text: only \ (backreferences) and the delimiter are
 * special. The _raw escapers omit double_trailing_esc so segments can be
 * concatenated with raw regex before the fixup is applied once to the whole. */
static char *escape_sub_repl_raw(const char *s, char delim)
{
	char set[3] = { '\\', delim, 0 };
	return escape_chars(s, set);
}

/* escape_regex plus the delimiter, for ex_re_read. */
static char *escape_sub_pat_raw(const char *s, char delim)
{
	char set[sizeof(REGEX_META) + 1];
	snprintf(set, sizeof(set), "%s%c", REGEX_META, delim);
	return escape_chars(s, set);
}

/* Longest common substring of a and b: its byte length, with ai/bi set to the
 * start offsets. Plain O(alen*blen) DP; diff lines are short. */
static int lcs_substr(const char *a, int alen, const char *b, int blen,
		      int *ai, int *bi)
{
	int best = 0;
	*ai = 0;
	*bi = 0;
	int row = blen + 1;
	int *prev = emalloc(row * sizeof(int));
	int *cur = emalloc(row * sizeof(int));
	memset(prev, 0, row * sizeof(int));
	for (int i = 1; i <= alen; i++) {
		cur[0] = 0;
		for (int j = 1; j <= blen; j++) {
			if (a[i - 1] == b[j - 1]) {
				cur[j] = prev[j - 1] + 1;
				if (cur[j] > best) {
					best = cur[j];
					*ai = i - cur[j];
					*bi = j - cur[j];
				}
			} else {
				cur[j] = 0;
			}
		}
		int *t = prev;
		prev = cur;
		cur = t;
	}
	free(prev);
	free(cur);
	return best;
}

/* A common block: identical run om[oa..oa+len) == nm[na..na+len), trimmed to
 * UTF-8 rune boundaries. */
typedef struct {
	int oa, na, len;
} block_t;

typedef struct {
	block_t *v;
	int n, cap;
} blockvec_t;

static void bv_add(blockvec_t *bv, int oa, int na, int len)
{
	ARR_PUSH(bv->v, bv->n, bv->cap)
	bv->v[bv->n].oa = oa;
	bv->v[bv->n].na = na;
	bv->v[bv->n++].len = len;
}

/* Decompose om/nm into in-order common blocks, difflib-style: longest common
 * substring, then recurse on the flanks. Blocks are rune-trimmed; the trimmed
 * edges and the gaps fall through as changed text. */
static void collect_blocks(const char *om, int os, int oe,
			   const char *nm, int ns, int ne, blockvec_t *bv)
{
	int alen = oe - os, blen = ne - ns;
	if (alen <= 0 || blen <= 0)
		return;
	int ai, bi;
	int L = lcs_substr(om + os, alen, nm + ns, blen, &ai, &bi);
	if (L <= 0)
		return;
	int bo = os + ai, bn = ns + bi;
	int s = 0, e = L;
	while (s < e && (om[bo + s] & 0xC0) == 0x80)
		s++;
	while (e > s && (om[bo + e] & 0xC0) == 0x80)
		e--;
	bo += s;
	bn += s;
	L = e - s;
	if (L <= 0)
		return;   /* whole block was a partial rune; treat as change */
	collect_blocks(om, os, bo, nm, ns, bn, bv);
	bv_add(bv, bo, bn, L);
	collect_blocks(om, bo + L, oe, nm, bn + L, ne, bv);
}

#define GRP_MIN_ISLAND 3   /* stable run must be >= this many runes to anchor a group */

/* Render mode for a stable run (capture group). */
enum { GM_LIT, GM_WILD, GM_FUZZ };   /* "(text)" / "(.*)" / "(head.*tail)" */

/* One token of the grp decomposition: a stable common run (a capture group) or
 * an edit (changed text matched literally on the old side, re-emitted on the
 * new side). Texts are borrowed slices of old/new. */
typedef struct {
	int stable;          /* 1 = stable anchor (group), 0 = edit */
	const char *o;
	int olen;   /* old text (pattern side) */
	const char *n;
	int nlen;   /* new text (replacement side) */
	int mode;            /* (stable) GM_LIT / GM_WILD / GM_FUZZ */
	int hb, tb;          /* (GM_FUZZ) head/tail byte lengths within o */
} gtok_t;

/* Byte length of the first k runes of [s,len] (clamped to len). */
static int rune_take(const char *s, int len, int k)
{
	int i = 0, n = 0;
	while (i < len && n < k) {
		i += uc_len((s + i));
		n++;
	}
	return i < len ? i : len;
}

/*
 * The MINIMAL head and tail (in runes) of a stable run that each occur exactly
 * once in the whole old line, so "(head.*tail)" matches it deterministically -
 * minimal anchors maximize the wildcarded interior. Needs non-overlapping
 * head/tail leaving at least one rune of middle; 0 = caller falls back to a
 * literal capture.
 */
static int fuzz_anchors(const char *old, int oldlen,
			const char *o, int olen, int *hb, int *tb)
{
	int R = rune_count_n(o, olen);
	int hk = 0, tk = 0, hbytes = 0, tbytes = 0;
	for (int k = 1; k <= R; k++) {
		hbytes = rune_take(o, olen, k);
		if (str_count_occ(old, oldlen, o, hbytes) == 1) {
			hk = k;
			break;
		}
	}
	if (!hk)
		return 0;
	for (int k = 1; k <= R; k++) {
		int off = rune_take(o, olen, R - k);
		tbytes = olen - off;
		if (str_count_occ(old, oldlen, o + off, tbytes) == 1) {
			tk = k;
			break;
		}
	}
	if (!tk || hk + tk >= R)   /* overlap or no interior left to absorb */
		return 0;
	*hb = hbytes;
	*tb = tbytes;
	return 1;
}

/*
 * Grp-capture absorbing substitute (rung 1 of the progression). The changed
 * line is decomposed into stable common runs and the edits between them, and
 * the pattern covers the EXACT SPAN ONLY - first edit to last edit. The stable
 * runs outside it are dropped: the substitute matches as an unanchored
 * substring, so wrapping the unchanged prefix/suffix in "(.*)" would only
 * duplicate the exact rung.
 *
 * Each in-span run becomes a capture group, each edit is matched literally and
 * re-emitted. A run is wildcarded so it absorbs drift inside itself:
 *   - full "(.*)" when flanked by non-empty edits whose old-texts are each
 *     UNIQUE in the old line, so the literal separators pin the greedy
 *     boundaries: "X bbbb Y" -> "P bbbb Q" gives s/X(.*)Y/P\1Q/.
 *   - else "(head.*tail)" over the minimal unique anchors (fuzz_anchors), for
 *     when a separator is an insertion or repeats and "(.*)" would be ambiguous
 *   - else a literal "(text)" capture.
 * Returns 0 unless at least one run is wildcarded: otherwise the variant
 * reproduces the span verbatim and is a pure dup of the exact rung.
 */
static int build_grp_variant(const char *old, const char *new,
			     char **pat_out, char **repl_out)
{
	*pat_out = NULL;
	*repl_out = NULL;
	int olen = strlen(old), nlen = strlen(new);
	blockvec_t bv = {0};
	collect_blocks(old, 0, olen, new, 0, nlen, &bv);

	/* Token stream: edits and substantial stable runs. Small common blocks
	 * are folded into the surrounding edit (pos not advanced past them). */
	gtok_t *tk = emalloc((bv.n * 2 + 2) * sizeof(gtok_t));
	int nt = 0, pos_o = 0, pos_n = 0;
	for (int i = 0; i < bv.n; i++) {
		block_t *b = &bv.v[i];
		/* Fold small INTERIOR runs into the surrounding edit, but keep the
		 * boundary ones regardless of size: they become "(.*)" absorbers, and
		 * folding one would strand an insertion against a wildcard and force
		 * the whole variant to be rejected. */
		int boundary = (i == 0 || i == bv.n - 1);
		if (!boundary && rune_count_n(old + b->oa, b->len) < GRP_MIN_ISLAND)
			continue;
		if (b->oa > pos_o || b->na > pos_n) {   /* edit gap before anchor */
			tk[nt].stable = 0;
			tk[nt].o = old + pos_o;
			tk[nt].olen = b->oa - pos_o;
			tk[nt].n = new + pos_n;
			tk[nt].nlen = b->na - pos_n;
			nt++;
		}
		tk[nt].stable = 1;
		tk[nt].o = old + b->oa;
		tk[nt].olen = b->len;
		tk[nt].n = new + b->na;
		tk[nt].nlen = b->len;
		nt++;
		pos_o = b->oa + b->len;
		pos_n = b->na + b->len;
	}
	if (olen > pos_o || nlen > pos_n) {   /* trailing edit */
		tk[nt].stable = 0;
		tk[nt].o = old + pos_o;
		tk[nt].olen = olen - pos_o;
		tk[nt].n = new + pos_n;
		tk[nt].nlen = nlen - pos_n;
		nt++;
	}
	free(bv.v);

	/* The exact span runs from the first edit to the last edit; only the
	 * stable runs strictly inside it are emitted (the rest is the unchanged
	 * prefix/suffix the substring match already skips). */
	int fe = -1, le = -1;
	for (int i = 0; i < nt; i++)
		if (!tk[i].stable) {
			if (fe < 0)
				fe = i;
			le = i;
		}
	if (fe < 0) {   /* no edit at all (old == new): nothing to do */
		free(tk);
		return 0;
	}
	int ns = 0;
	for (int i = fe + 1; i < le; i++)
		if (tk[i].stable)
			ns++;
	if (ns == 0 || ns > 9) {   /* no in-span run, or > \1..\9 backref limit */
		free(tk);
		return 0;
	}

	/* Per-stable cumulative old-text length of the edit gap immediately to its
	 * left and right; a non-empty gap is a literal separator that disambiguates
	 * an adjacent full "(.*)". */
	int *lgap = emalloc(nt * sizeof(int)), *rgap = emalloc(nt * sizeof(int));
	int run = 0;
	for (int i = 0; i < nt; i++) {
		if (tk[i].stable) {
			lgap[i] = run;
			run = 0;
		} else
			run += tk[i].olen;
	}
	run = 0;
	for (int i = nt - 1; i >= 0; i--) {
		if (tk[i].stable) {
			rgap[i] = run;
			run = 0;
		} else
			run += tk[i].olen;
	}

	int absorb = 0;
	for (int i = fe + 1; i < le; i++) {
		if (!tk[i].stable)
			continue;
		if (lgap[i] > 0 && rgap[i] > 0 &&
		    str_count_occ(old, olen, tk[i-1].o, tk[i-1].olen) == 1 &&
		    str_count_occ(old, olen, tk[i+1].o, tk[i+1].olen) == 1) {
			/* Full "(.*)": its boundaries are pinned by the literal edit
			 * old-text on each side, so both separators must be unique in
			 * the old line or the greedy ".*" split is ambiguous. */
			tk[i].mode = GM_WILD;
			absorb = 1;
		} else if (fuzz_anchors(old, olen, tk[i].o, tk[i].olen,
					&tk[i].hb, &tk[i].tb)) {
			tk[i].mode = GM_FUZZ;       /* head/tail anchored absorber */
			absorb = 1;
		} else {
			tk[i].mode = GM_LIT;        /* no unique minimal anchors */
		}
	}
	free(lgap);
	free(rgap);
	if (!absorb) {   /* reproduces the span verbatim: a dup of the exact rung */
		free(tk);
		return 0;
	}

	sbuf_smake(pat, 128)
	sbuf_smake(repl, 128)
	int g = 0;
	for (int i = fe; i <= le; i++) {
		if (tk[i].stable) {
			char br[16];
			snprintf(br, sizeof(br), "\\%d", ++g);
			if (tk[i].mode == GM_WILD) {
				sbuf_str(pat, "(.*)")
			} else if (tk[i].mode == GM_FUZZ) {
				char *h = dup_n(tk[i].o, tk[i].hb);
				char *t = dup_n(tk[i].o + tk[i].olen - tk[i].tb,
						tk[i].tb);
				char *eh = escape_sub_pat_raw(h, '/');
				char *et = escape_sub_pat_raw(t, '/');
				sbuf_chr(pat, '(')
				sbuf_str(pat, eh)
				sbuf_str(pat, ".*")
				sbuf_str(pat, et)
				sbuf_chr(pat, ')')
				free(eh);
				free(et);
				free(h);
				free(t);
			} else {
				char *tmp = dup_n(tk[i].o, tk[i].olen);
				char *e = escape_sub_pat_raw(tmp, '/');
				sbuf_chr(pat, '(')
				sbuf_str(pat, e)
				sbuf_chr(pat, ')')
				free(e);
				free(tmp);
			}
			sbuf_str(repl, br)
		} else {
			char *to = dup_n(tk[i].o, tk[i].olen);
			char *tn = dup_n(tk[i].n, tk[i].nlen);
			char *pe = escape_sub_pat_raw(to, '/');
			char *re = escape_sub_repl_raw(tn, '/');
			sbuf_str(pat, pe)
			sbuf_str(repl, re)
			free(pe);
			free(re);
			free(to);
			free(tn);
		}
	}
	free(tk);
	sbuf_nul4(pat)
	sbuf_nul4(repl)
	*pat_out = double_trailing_esc(pat->s);
	*repl_out = double_trailing_esc(repl->s);
	return 1;
}


/* One progression rung, from pre-escaped pat/repl. */
static void emit_substitute_grp(sbuf *out, const char *pat, const char *repl)
{
	sb_str(out, "s/");
	sb_exarg(out, pat);
	sb_chr(out, '/');
	sb_exarg(out, repl);
	sb_chr(out, '/');
}

/* One rung of the phase-2 substitute progression: a fully-escaped s/// pair. */
typedef struct {
	char *pat;
	char *repl;
	int sid;
} subvar_t;

/*
 * Phase 2 substitute progression, mirroring emit_fallback_chain: each variant
 * (exact -> grp-absorbing) at the mark, first success wins. The s/// is both
 * test and action - a failed match leaves the line untouched, so the next,
 * looser variant is safe to try, and the first success short-circuits with 1q
 * so no later one re-edits the now changed line. A non-primary success reports
 * via ${OK2}; if every variant fails the trailing DNF check reports FAIL. A
 * single-variant chain degrades to a plain addressed s/// + check.
 */
static void emit_substitute_chain(sbuf *out, int line, int mark_id,
				  subvar_t *v, int nv)
{
	int sids[NSEARCH];
	if (nv <= 1) {
		sb_printf(out, "'%d", mark_id);
		emit_substitute_grp(out, v[0].pat, v[0].repl);
		EMIT_SEP(out);
		emit_err_check(out, 2, line, mark_id, NULL, 0);
		return;
	}
	sb_chr(out, '?');
	for (int n = 0; n < nv; n++) {
		if (n)
			EMIT_ESCSEP(out);
		/* action: substitute at the mark (status tested below) */
		sb_printf(out, "'%d", mark_id);
		emit_substitute_grp(out, v[n].pat, v[n].repl);
		EMIT_ESCSEP(out);
		sb_printf(out, "%d??", v[n].sid);   /* capture s/// status into tag */
		EMIT_ESCSEP(out);
		sb_printf(out, "%d??", v[n].sid);   /* on success (fire): */
		if (n) {
			/* harmless mark jump as the immediate then-arg keeps
			 * the OK report non-immediate (mirrors phase 1's
			 * report after "m id") */
			sb_printf(out, "'%d", mark_id);
			EMIT_ESC3SEP(out);
			sb_printf(out, "%dreg p OK %s:%d:s%d", REG_MSG,
				  cur_file_path ? cur_file_path : "?", line, v[n].sid);
			EMIT_ESC3SEP(out);
			emit_reg_call(out, REG_OK2, 1);
			if (n < nv - 1) {
				EMIT_ESC3SEP(out);
				sb_str(out, "1q");
			}
		} else {
			sb_str(out, "1q");
		}
	}
	EMIT_SEP(out);
	EMIT_LB(out);
	EMIT_SEP(out);
	for (int n = 0; n < nv; n++)
		sids[n] = v[n].sid;
	emit_err_check(out, 2, line, mark_id, sids, nv);
	EMIT_LB(out);
	EMIT_SEP(out);
}

/* The substitute progression for a single-line change: rung 0 the minimal-span
 * exact s/old/new/, rung 1 the grp-absorbing variant over that same span
 * (skipped when it would absorb nothing). Fields are fully escaped; caller
 * frees. */
static int build_sub_variants(group_t *g, subvar_t *v)
{
	int nv = 0;
	/* rung 0: the minimal-span exact substitute, fully escaped */
	v[nv].pat = double_trailing_esc(escape_sub_pat_raw(g->ld_old_text, '/'));
	v[nv].repl = double_trailing_esc(escape_sub_repl_raw(g->ld_new_text, '/'));
	v[nv].sid = 1;
	nv++;
	if (build_grp_variant(g->del_texts[0], g->add_texts[0],
			      &v[nv].pat, &v[nv].repl)) {
		v[nv].sid = 2;
		nv++;
	}
	return nv;
}

/* Phase 2: substitute at a mark, through the exact -> grp-absorbing chain. */
static void emit_mark_substitute(sbuf *out, int line, int mark_id,
				 group_t *g)
{
	subvar_t v[2];
	int nv = build_sub_variants(g, v);
	emit_substitute_chain(out, line, mark_id, v, nv);
	for (int i = 0; i < nv; i++) {
		free(v[i].pat);
		free(v[i].repl);
	}
}

/* Editor bring-up, hoisted from nextvi's main()/ex_init(): no argv, no EXINIT,
 * for the sessions that edit buffers patch2vi built rather than files (-E goes
 * through nextvi_main() instead). Split into init/teardown so one process can
 * run several independent editor lifetimes - one per script block, the -e
 * runner. The config tables and the input buffer are
 * process-wide and built once; the rest is per session, freed by ed_free().
 * With use_tty the editor takes the controlling terminal on fds 0/1, since
 * patch2vi's own stdin/stdout may be the patch and the generated script. */
static int ed_in = -1, ed_out = -1;
static int ed_once;	/* the process-wide half of the bring-up is done */

/* Put the controlling terminal on fds 0/1 for the session; ed_ungrabtty()
 * puts the caller's own stdin/stdout back. */
static int ed_grabtty(void)
{
	int tty = open("/dev/tty", O_RDWR);
	if (tty < 0) {
		perror("/dev/tty");
		return -1;
	}
	fflush(stdout);
	ed_in = dup(0);
	ed_out = dup(1);
	dup2(tty, 0);
	dup2(tty, 1);
	close(tty);
	return 0;
}

static void ed_ungrabtty(void)
{
	fflush(stdout);
	if (ed_in >= 0) {
		dup2(ed_in, 0);
		dup2(ed_out, 1);
		close(ed_in);
		close(ed_out);
		ed_in = ed_out = -1;
	}
}

static int ed_init(int use_tty)
{
	if (use_tty && ed_grabtty() < 0)
		return -1;
	if (!ed_once) {
		setup_signals();
		dir_init();
		syn_init();
		ibuf = emalloc(ibuf_sz);
		ed_once = 1;
	}
	temp_open(0, "/hist/", _ft);
	temp_open(1, "/fm/", fm_ft);
	temp_open(2, "/sc/", _ft);
	term_init();
	ec_setbufsmax(NULL, NULL, "");
	xmpt = 0;
	return 0;
}

/* End the session and report it as nextvi's main() would */
static int ed_done(void)
{
	int st;
	term_done();
	st = xquit < -256 ? (abs(xquit) - 257) & 255 : abs(xquit) - 1;
	xquit = 0;
	ed_ungrabtty();
	return st;
}

/* Everything that is not a buffer: temp buffers, registers, the "??" tags, the
 * last search and the globals a body may have changed. Dropped between blocks
 * even where the buffers persist (replay), so nothing is inherited. */
static void ed_free_session(void)
{
	int i;
	/* a run that dies before ed_setup() (a script that will not even parse)
	 * still unwinds through here, and so does a second free: both leave the
	 * temp buffers zeroed, which lbuf_free() does not take */
	for (i = 0; i < (int)LEN(tempbufs); i++) {
		free(tempbufs[i].path);
		if (tempbufs[i].lb)
			lbuf_free(tempbufs[i].lb);
		memset(&tempbufs[i], 0, sizeof(tempbufs[i]));
	}
	for (i = 0; i < xregs_n; i++)
		if (xregs[i])
			sbuf_free(xregs[i])
	free(xregs);
	xregs = NULL;
	xregs_n = 0;
	if (xanchor) {
		sbuf_free(xanchor)
		xanchor = NULL;
	}
	if (xacreg) {
		sbuf_free(xacreg)
		xacreg = NULL;
	}
	rset_free(xkwdrs);
	xkwdrs = NULL;
	xrow = xoff = xtop = 0;
	xleft = 0;
	xquit = xgrec = xgdep = xexec_dep = 0;
	xkwddir = xkwdcnt = 0;
	xfr = xrr = xpr = xgrp = xdefreg = 0;
	xpret = NULL;
	xsep = ':';
	xesc = '\\';
	xerr = 1;
	xseq = 1;
	xvis = 0;
	/* ignorecase defaults on in nextvi and every pattern here is literal
	 * source text, so a replay session must turn it off too, whether or
	 * not a replayed prologue already did. */
	xic = 0;
}

/* The state above plus the buffers, their marks and their undo history: a block
 * started after this sees exactly what a freshly spawned editor sees. */
static void ed_free_bufs(void)
{
	for (int i = 0; i < xbufcur; i++)
		bufs_free(i);
	xbufcur = 0;
	free(bufs);
	bufs = NULL;
	ex_buf = ex_pbuf = ex_tpbuf = NULL;
	xbufsmax = 0;
}

static void ed_free(void)
{
	ed_free_bufs();
	ed_free_session();
}

/* Load one in-memory buffer under a label; nothing here touches the
 * filesystem. */
static void ed_loadbuf(const char *name, char *text)
{
	char msg[512];
	bufs_switch(bufs_open(name, strlen(name)));
	lbuf_edit(xb, text, 0, 0, 0, 0);
	ex_bufpostfix(ex_buf, 1);
	snprintf(msg, sizeof(msg), "\"%s\" %dL [f]", xb_path, lbuf_len(xb));
	/* "-m" silences the load line, as ec_edit's own is */
	if (!(xvis & 4))
		ex_print(msg, bar_ft)
}

/* The nextvi command line that follows a mode's own arguments: -E's after
 * its script, -C's after its fix slot. Its option letters are vi(1)'s own,
 * applied to the handover session, and its files are opened on top of the
 * ones the run itself named - so a session can visit a file the script never
 * touched and still have it end up in the emitted diff. */
static int hand_vis = -1;	/* xvis for the session, -1 = plain visual */
static char **hand_files;
static int nhand_files;

/* The one editor session every handover path ends in. Undo what the
 * replay or the loader left behind (the body's "|sc!" separator, escape
 * and error mode), open the command line's files, park on the first placed
 * failure (fbuf < 0 when there is none), fire the P2VI_EX harness hook and
 * enter the loop nextvi_main() would have - the parsed flags decide, not a
 * hardcoded vi(): "-e" and "-s" mean ex(), "-a" wraps the session in the
 * scroll history. Startup cannot be redone here (term_init already ran
 * without bit 1, and the buffers are loaded, so ex_init gets no argv),
 * which is why this mirrors main()'s tail instead of calling any of its. */
static void ed_serve(int fbuf, int frow)
{
	char *ln;
	int k;

	xvis = hand_vis >= 0 ? hand_vis : 0;
	xsep = ':';
	xesc = '\\';
	xerr = 1;
	for (k = 0; k < nhand_files; k++)
		ec_edit("", "e", hand_files[k]);
	if (fbuf >= 0 && fbuf < xbufcur) {
		bufs_switch(fbuf);
		xrow = frow;
		xoff = 0;
	}
	syn_setft(xb_ft);
	if ((ln = getenv("P2VI_EX")))	/* test harness hook */
		ex_command(ln)
	/* like ex_init(): never enter vi() with xmpt > 1, or it opens with
	 * the "[any key to continue]" pager (each load print bumps xmpt) */
	if (xmpt > 1)
		xmpt = 1;
	if (!xquit) {
		if (xvis & 8)
			term_scrh()
		if (xvis & 2)
			ex();
		else
			vi(1);
		if (xvis & 8)
			term_scrl()
	}
}

/* A buffer's content as one heap-allocated string */
static char *lbuf_text(struct lbuf *lb)
{
	char *ln;
	sbuf_smake(sb, SB_INIT)
	for (int i = 0; i < lbuf_len(lb); i++) {
		ln = lbuf_get(lb, i);
		sbuf_mem(sb, ln, lbuf_s(ln)->len + 1)
	}
	sbufn_ret(sb, sb->s)
}

/* One derived (or re-read) compatibility block: one whole compat patch, i.e.
 * one unified diff over however many files it touches. One block = one section
 * = one staged body = one storage region, so a compat patch is authored and
 * shipped as the single diff it is. Its body always runs after the host's;
 * origin is per-block, since the global only describes the current run. */
typedef struct {
	char *origin;		/* src= label; its basenames are the identity gate */
	int first, count;	/* files[] range this block owns */
	strv_t raw;		/* the block's own === PATCH === lines */
} compat_block_t;
static compat_block_t *compat_blocks;
static int ncompat, compat_cap;

/* Forward declaration: the -e applied set needs basenames first. */
static const char *base_name(const char *p);
/* the src= fields of a compat block's origin label, as basenames */
static int compat_src_fields(compat_block_t *cb, char ***out);

/* The block's own files that have groups to emit; *n = how many. */
static file_patch_t **block_files(compat_block_t *cb, int *n)
{
	file_patch_t **v = emalloc((cb->count + 1) * sizeof(*v));
	*n = 0;
	for (int i = cb->first; i < cb->first + cb->count; i++)
		if (files[i].ngroups > 0)
			v[(*n)++] = &files[i];
	return v;
}

/* Enter/leave the compat emission window: relative anchoring and the
 * file-validated generators off (they would read the pre-origin file). Held
 * around a block's body emission. */
static void compat_win_enter(int *sv)
{
	*sv = relative_mode;
	relative_mode = 1;
	compat_building = 1;
}

static void compat_win_leave(int sv)
{
	compat_building = 0;
	relative_mode = sv;
}

/*
 * groups[] for a file, from its ops[]: a group is a contiguous run of
 * deletes/adds with optional context anchors.
 */
static void build_file_groups(file_patch_t *fp)
{
	int rel = relative_mode;

	if (fp->nops == 0)
		return;
	/* A compat block's own files collect their context search-anchored
	 * whatever the host's mode is: anchors are what make a block
	 * survive the chains it is gated for. */
	for (int c = 0; c < ncompat; c++)
		if (fp - files >= compat_blocks[c].first &&
		    fp - files < compat_blocks[c].first + compat_blocks[c].count) {
			rel = 1;
			break;
		}

	fp->groups = ecalloc(fp->nops + 1, sizeof(group_t));
	group_t *groups = fp->groups;
	int ngroups = 0;
	int i = 0;

	while (i < fp->nops) {
		group_t *g = &groups[ngroups];
		memset(g, 0, sizeof(group_t));

		/* Consume the context run before the change: its last line
		 * measures the distance to the first change, and its last
		 * three lines become the search pattern's leading lines. */
		int last_ctx_line = 0;
		char *ctx_ring[3] = {NULL, NULL, NULL};
		int ctx_line_ring[3] = {0, 0, 0};
		int ctx_count = 0;
		while (i < fp->nops && fp->ops[i].type == 'c') {
			last_ctx_line = fp->ops[i].oline;
			ctx_ring[0] = ctx_ring[1];
			ctx_line_ring[0] = ctx_line_ring[1];
			ctx_ring[1] = ctx_ring[2];
			ctx_line_ring[1] = ctx_line_ring[2];
			ctx_ring[2] = fp->ops[i].text;
			ctx_line_ring[2] = fp->ops[i].oline;
			ctx_count++;
			i++;
		}
		if (i >= fp->nops)
			break;

		if (last_ctx_line) {
			int first_change_line = fp->ops[i].oline;
			g->anchor_offset = first_change_line - last_ctx_line;
		}
		if (ctx_count >= 3) {
			g->anchors[0] = ctx_ring[0];
			g->anchors[1] = ctx_ring[1];
			g->anchors[2] = ctx_ring[2];
			g->nanchors = 3;
		} else if (ctx_count == 2) {
			g->anchors[0] = ctx_ring[1];
			g->anchors[1] = ctx_ring[2];
			g->nanchors = 2;
		} else if (ctx_count == 1) {
			g->anchors[0] = ctx_ring[2];
			g->nanchors = 1;
		}

		/* The original-line span of the enclosing @@ hunk: the
		 * search windows that absorb it must anchor outside it. */
		g->hunk_lo = fp->ops[i].hunk_lo;
		g->hunk_hi = fp->ops[i].hunk_hi;

		int del_start_idx = i;
		if (fp->ops[i].type == 'd') {
			g->del_start = fp->ops[i].oline;
			g->del_end = fp->ops[i].oline;
			i++;
			while (i < fp->nops && fp->ops[i].type == 'd' &&
			       fp->ops[i].oline == g->del_end + 1) {
				g->del_end = fp->ops[i].oline;
				i++;
			}
		}
		g->ndel = i - del_start_idx;
		g->del_texts = emalloc(g->ndel * sizeof(char*));
		for (int j = 0; j < g->ndel; j++)
			g->del_texts[j] = fp->ops[del_start_idx + j].text;

		int add_start = i;
		while (i < fp->nops && fp->ops[i].type == 'a')
			i++;
		g->nadd = i - add_start;
		if (g->nadd > 0) {
			g->add_texts = emalloc(g->nadd * sizeof(char*));
			for (int j = 0; j < g->nadd; j++)
				g->add_texts[j] = fp->ops[add_start + j].text;
			if (g->del_start == 0) {
				g->add_after = fp->ops[add_start].oline - 1;
			}
		}

		/* The first following context line, and its distance to the
		 * first change: the pattern's trailing anchor. */
		if (i < fp->nops && fp->ops[i].type == 'c') {
			g->follow_ctx = fp->ops[i].text;
			int first_change_line = g->del_start ? g->del_start : g->add_after + 1;
			g->follow_offset = fp->ops[i].oline - first_change_line;
		}

		/* Relative mode keeps up to three following context lines on
		 * top of follow_ctx: absolute line numbers need no context,
		 * search patterns do. */
		if (rel && (g->del_start || g->nadd)) {
			int post_cap = 3;
			int post_avail = 0;
			int pi = i;
			while (pi < fp->nops && fp->ops[pi].type == 'c' && post_avail < post_cap) {
				post_avail++;
				pi++;
			}
			if (post_avail > 0) {
				g->post_ctx = emalloc(post_avail * sizeof(char*));
				g->npost_ctx = post_avail;
				for (int j = 0; j < post_avail; j++)
					g->post_ctx[j] = fp->ops[i + j].text;
			}
		}
		/* A single-line change precomputes its character-level diff:
		 * both the substitute and the character-level edit shape are
		 * cut from it. */
		if (g->ndel == 1 && g->nadd == 1 &&
		    g->del_texts[0] && g->add_texts[0]) {
			g->has_line_diff = find_line_diff(
						   g->del_texts[0], g->add_texts[0],
						   &g->ld_old_text, &g->ld_new_text);
			if (g->has_line_diff) {
				/* The character-level shape keeps the minimal
				 * span - no uniqueness expansion, it addresses
				 * the line directly - and its positions are
				 * rune indexes, which a split rune would shift
				 * and splice into invalid UTF-8. */
				const char *old = g->del_texts[0];
				const char *new = g->add_texts[0];
				int olen = strlen(old), nlen = strlen(new);
				int prefix, suffix;
				common_affix(old, new, &prefix, &suffix);
				g->ldc_start = rune_count_n(old, prefix);
				g->ldc_end = rune_count_n(old, olen - suffix);
				g->ldc_new_text = dup_n(new + prefix,
							nlen - suffix - prefix);
			}
		}

		if (g->del_start || g->nadd)
			ngroups++;
	}
	fp->ngroups = ngroups;
}

/* Free a group's heap data after emission */
static void free_group(group_t *g)
{
	free(g->del_texts);
	free(g->add_texts);
	free(g->post_ctx);
	free(g->ld_old_text);
	free(g->ld_new_text);
	free(g->ldc_new_text);
	free(g->ph1_gen);
	free(g->ph2_gen);
}

/* Every group's verbatim phase-1/phase-2 segment bytes into ph1_gen/ph2_gen
 * (forward layout only). */
static void gen_group_segments(file_patch_t *fp)
{
	group_t *groups = fp->groups;
	int ngroups = fp->ngroups;

	cur_file_path = fp->path;

	/* the pre-patch original, for anchor validation: the "---" path
	 * names it, and the edit target holds the same content until the
	 * script runs */
	if (!fp->is_new && !compat_building)
		load_orig_file(fp->orig_path ? fp->orig_path : fp->path);

	/* Phase 1 (resolve): every group's search against the register cache,
	 * recording its target line in a mark. Edit marks start at 1, mark 0
	 * being the global searches' cursor scratch. */
	int next_id = WIN_SAVE_MARK + 1;
	int first_search = 1;
	for (int gi = 0; gi < ngroups; gi++) {
		group_t *g = &groups[gi];
		g->mark_id = -1;
		g->insert_i = 0;
		if (!g->del_start && !g->nadd)
			continue;
		sbuf_smake(out, SB_INIT)
		int target_line = g->del_start ? g->del_start : g->add_after;

		int has_anchors = group_has_anchors(g);
		int strat = (relative_mode && has_anchors) ? STRAT_REL : STRAT_ABS;
		g->res_strat = strat;

		if (strat == STRAT_ABS) {
			/* New file: empty buffer, nothing to mark; phase 2
			 * emits a bare i (mark_id stays -1) */
			if (fp->is_new && !g->del_start) {
				g->insert_i = 1;
				goto ph1_done;
			}
			int t = target_line;
			if (!g->del_start && t <= 0) {
				t = 1;
				g->insert_i = 1;
			}
			g->mark_id = next_mark_id(&next_id);
			sb_printf(out, "%dm %d", t, g->mark_id);
			EMIT_SEP(out);
			goto ph1_done;
		}

		/* The fallback list: the default patterns, then the
		 * file-validated relaxed windows. Duplicates dropped. */
		pat_spec_t ps[NSEARCH];
		int nps = 0;
		int slot_sz = g->ndel + 7;
		char **raw = emalloc(NPAT * slot_sz * sizeof(char *));
		winset_t ws;           /* owned relaxed windows */
		memset(&ws, 0, sizeof(ws));
		for (int pi = 0; pi < NPAT; pi++) {
			char **slot = raw + pi * slot_sz;
			int doff;
			int n = default_pat_lines(g, pi, slot, &doff);
			if (!n)
				continue;
			ps[nps].lines = slot;
			ps[nps].nlines = n;
			ps[nps].pre_escaped = 0;
			ps[nps].offset = doff;
			ps[nps].off_final = 0;
			ps[nps].mode = n == 1 ? 1 : 0;
			ps[nps].pid = pi + 1;
			nps++;
		}
		/* the relaxed windows, loosest last; off_final on the
		 * last three keeps their offsets through the pure-add shift */
		gen_extra_windows(g, &ws);
		for (int pi = NPAT; pi < NSEARCH && nps < NSEARCH; pi++)
			if (ws.has[pi - NPAT])
				nps = push_win_pat(ps, nps, &ws.w[pi - NPAT],
						   pi + 1, pi >= GRP_SLOT);
		/* No re-sort: the slots are already strict to loose. */
		int w = 0;
		for (int pi = 0; pi < nps; pi++) {
			int dup = 0;
			for (int pj = 0; pj < w; pj++)
				if (ps[pi].pre_escaped == ps[pj].pre_escaped &&
				    lines_equal(ps[pi].lines, ps[pi].nlines,
						ps[pj].lines, ps[pj].nlines))
					dup = 1;
			if (!dup)
				ps[w++] = ps[pi];
		}
		nps = w;

		/* Pure insert: the mark lands on the line to append after. */
		if (!g->del_start && g->nadd) {
			if (g->add_after <= 0)
				g->insert_i = 1;
			else
				for (int pi = 0; pi < nps; pi++)
					if (!ps[pi].off_final)
						ps[pi].offset -= 1;
		}

		g->mark_id = next_mark_id(&next_id);
		if (nps == 0) {
			/* No usable anchor: mark the absolute line */
			sb_printf(out, "%dm %d",
				  target_line > 0 ? target_line : 1, g->mark_id);
			EMIT_SEP(out);
			free_extra_windows(&ws);
			free(raw);
			goto ph1_done;
		}
		if (nps == 1)
			emit_search(out, ps[0].lines, ps[0].nlines,
				    ps[0].offset, g->mark_id, target_line,
				    ps[0].pre_escaped, first_search, ps[0].mode);
		else
			emit_fallback_chain(out, ps, nps, g->mark_id,
					    target_line, first_search);
		first_search = 0;
		free_extra_windows(&ws);
		free(raw);
ph1_done:
		sbuf_nul(out)
		g->ph1_gen = out->s;
	}

	/* Phase 2 (commit): the edits at their marks, forward order; the marks
	 * auto-adjust as edits above them shift lines. */

	for (int gi = 0; gi < ngroups; gi++) {
		group_t *g = &groups[gi];
		if (!g->del_start && !g->nadd)
			continue;
		sbuf_smake(out, SB_INIT)
		int strat = g->res_strat;
		int tline = g->del_start ? g->del_start : g->add_after;
		EMIT_LB(out);
		EMIT_SEP(out);

		if (g->del_start && g->nadd) {
			if (strat == STRAT_REL && g->ndel == 1 && g->nadd == 1
			    && g->has_line_diff) {
				emit_mark_substitute(out, tline, g->mark_id, g);
			} else if (strat == STRAT_ABS && g->ndel == 1 && g->nadd == 1
				   && g->has_line_diff) {
				sb_printf(out, "'%d", g->mark_id);
				emit_horiz_span(out, g->ldc_start, g->ldc_end,
						g->ldc_new_text);
				emit_err_check(out, 2, tline, g->mark_id, NULL, 0);
			} else {
				emit_mark_change(out, tline, g->mark_id,
						 g->ndel, g->add_texts, g->nadd);
			}
		} else if (g->del_start) {
			emit_mark_delete(out, tline, g->mark_id, g->ndel);
		} else if (g->nadd) {
			emit_mark_insert(out, tline, g->mark_id, g->insert_i,
					 g->add_texts, g->nadd);
		}
		sbuf_nul(out)
		g->ph2_gen = out->s;
	}
	free_orig_file();
}

/* One file's groups as ex commands: absolute mode bottom-to-top (line numbers
 * stay valid, no searches, no marks), relative mode every phase-1 segment
 * and then every phase-2 one. The groups are freed here. */
static void emit_file_script(sbuf *out, file_patch_t *fp)
{
	if (fp->ngroups == 0)
		return;

	group_t *groups = fp->groups;
	int ngroups = fp->ngroups;

	if (!relative_mode) {
		/* Absolute mode: reverse order (bottom-to-top) preserves
		 * line numbers; no searches, no marks. */
		for (int gi = ngroups - 1; gi >= 0; gi--) {
			group_t *g = &groups[gi];
			if (g->del_start && g->nadd) {
				if (g->ndel == 1 && g->nadd == 1 && g->has_line_diff)
					emit_horizontal_change(out, g->del_start,
							       g->ldc_start, g->ldc_end,
							       g->ldc_new_text);
				else
					emit_change(out, g->del_start, g->del_end,
						    g->add_texts, g->nadd);
			} else if (g->del_start) {
				emit_delete(out, g->del_start, g->del_end);
			} else if (g->nadd) {
				emit_insert_after(out, g->add_after,
						  g->add_texts, g->nadd,
						  fp->is_new);
			}
			free_group(g);
		}
		return;
	}

	gen_group_segments(fp);
	for (int gi = 0; gi < ngroups; gi++)
		if (groups[gi].ph1_gen)
			sb_seg(out, groups[gi].ph1_gen);
	for (int gi = 0; gi < ngroups; gi++)
		if (groups[gi].ph2_gen)
			sb_seg(out, groups[gi].ph2_gen);
	for (int gi = 0; gi < ngroups; gi++)
		free_group(&groups[gi]);
}

/* One file inside a body: select its buffer, yank it into the find register so
 * every relative search of this file runs against a cache that stays
 * byte-identical to the pristine buffer (a file the patch creates has nothing
 * to cache), then its groups. */
static void emit_file_body(sbuf *out, file_patch_t *fp, int buf, int cache)
{
	sb_printf(out, "b%d", buf);
	EMIT_SEP(out);
	if (cache) {
		sb_str(out, "%ya 98");
		EMIT_SEP(out);
	}
	cur_file_path = fp->path;
	emit_file_script(out, fp);
}

/* The state registers, defined at the top of every $VI body: the body writes
 * the default state and the shell then contributes whole commands that flip
 * individual switches (any non-empty value counts as set).
 * REG_HDLR prints the FAIL line with the print redirected into REG_FLOG, so the
 * line reaches the terminal and the log both, and then calls INTR; a phase's
 * FAIL chain calls REG_HDLR and then its quit chain, so QF only fires where the
 * report does. The redirect is switched off again right after ("pr" with no
 * argument toggles), leaving the log the only state the site keeps. */
static void emit_qf2_assert(sbuf *out);

static void emit_reg_defaults(sbuf *out)
{
	sb_printf(out, "%dreg pr%c", REG_HDLR, REG_FLOG);
	EMIT_ESCSEP(out);
	sb_printf(out, "p FAIL %%@%d", REG_LOC);
	EMIT_ESCSEP(out);
	sb_str(out, "pr");
	EMIT_ESCSEP(out);
	sb_printf(out, "? %%@%d", REG_INTR);
	EMIT_SEP(out);
	/* phase 2 reports and quits by default, phase 1 does neither */
	sb_printf(out, "%dreg ? %%@%d", REG_ERR2, REG_HDLR);
	EMIT_ESCSEP(out);
	sb_printf(out, "? %%@%d", REG_QF2);
	EMIT_SEP(out);
	sb_printf(out, "%dreg ? %%@%d", REG_OK2, REG_MSG);
	EMIT_SEP(out);
	sb_printf(out, "%dreg vis 2", REG_QF2A);
	EMIT_ESCSEP(out);
	sb_str(out, "q!1");
	EMIT_SEP(out);
	/* the default assert, the same one a -C block restores */
	emit_qf2_assert(out);
}

/* The switches, as whole commands the shell either contributes or not - the
 * only part of a body sh writes, and a double-quoted word, so its raw separator
 * bytes are escaped for it. The backslash-newline breaks are continuations
 * inside the quotes, so they only split the word for readability.
 *
 * applied adds the applied set to that word: ex can read no environment but
 * EXINIT, so $P2VI_PATCH has to be written in from outside, and this word is
 * already the place the shell writes. The value lands in REG_APPLIED with a
 * space at each end - two spaces after "reg", since ex_cmd eats one - so the
 * identity gates emit_compat_gates builds delimit a name with plain spaces. It
 * goes out ahead of the body, which is the third printf argument, so the
 * register is set before anything reads it. */
static void emit_reg_switches(sbuf *out, int applied)
{
	if (applied) {
		sb_printf(out, "%dreg  $P2VI_PATCH ", REG_APPLIED);
		sb_dq_esc_sep(out, 0);
		sb_str(out, "\\\n");
	}
	sb_printf(out, "${DBG1:+%dreg ? %%@%d", REG_ERR1, REG_HDLR);
	sb_dq_esc_sep(out, 1);
	sb_printf(out, "? %%@%d", REG_QF1);
	sb_dq_esc_sep(out, 0);
	sb_printf(out, "%dreg ? %%@%d", REG_OK1, REG_MSG);
	sb_dq_esc_sep(out, 0);
	sb_str(out, "}\\\n");
	sb_printf(out, "${DBG2:+ya!%d", REG_ERR2);
	sb_dq_esc_sep(out, 0);
	sb_printf(out, "ya!%d", REG_OK2);
	sb_dq_esc_sep(out, 0);
	sb_str(out, "}\\\n");
	sb_printf(out, "${QF1:+%dreg vis 2", REG_QF1);
	sb_dq_esc_sep(out, 1);
	sb_str(out, "q!1");
	sb_dq_esc_sep(out, 0);
	sb_str(out, "}\\\n");
	sb_printf(out, "${QF2:+ya!%d", REG_QF2A);
	sb_dq_esc_sep(out, 0);
	sb_str(out, "}\\\n");
	/* the failing site is where the script wrote the location
	 * register, so INTR searches itself for that argument */
	sb_printf(out, "${INTR:+%dreg |sc|", REG_INTR);
	sb_dq_esc_sep(out, 1);
	sb_printf(out, "vis 2:fr 0:e $0:83reg %%@47:%%f> %dreg %%@%d:&Q:b0:"
		  "|sc! ", REG_LOC, REG_LOC);
	sb_dq_esc_sep(out, 3);
	sb_str(out, "|:vis 3");
	sb_dq_esc_sep(out, 1);
	sb_str(out, "q1");
	sb_dq_esc_sep(out, 0);
	sb_str(out, "}");
}

/* The tail a $VI body ends with: leave raw ex mode, write each of the nbufs
 * real files (b0..bN-1, the order the call opened them in) and quit. skip, when
 * given, marks buffers a section body writes itself (a compat-only file, which
 * must not be created when its identity gate misses) - the tail skips them. */
static void emit_write_tail(sbuf *out, int nbufs, const char *skip)
{
	sb_str(out, "vis 2");
	EMIT_SEP(out);
	for (int i = 0; i < nbufs; i++) {
		if (skip && skip[i])
			continue;
		sb_printf(out, "b%d", i);
		EMIT_SEP(out);
		sb_str(out, "w");
		EMIT_SEP(out);
	}
	sb_str(out, "2q");
}

/* The specials prologue every body opens with: "|sc! <esc><sep>|" declares the
 * escape and separator bytes to ex (with the default backslash escape the loc
 * halves a doubled one), "vis 3" enters raw ex mode, "ic 0" makes every pattern
 * case-sensitive.
 *
 * That last one is a correctness requirement, not a preference: nextvi's
 * ignorecase defaults ON and feeds every rset this script relies on, while the
 * patterns here are literal source text. A case-folded match is simply the
 * wrong line or the wrong byte - "s/x/w/" meant for "xrows" lands on the "X" of
 * an earlier "MAX(" - and the shorter the pattern, the likelier the impostor. */
static void emit_prologue(sbuf *out)
{
	sb_str(out, "|sc! ");
	sb_chr(out, dyn_esc ? dyn_esc : '\\');
	if (!dyn_esc)
		sb_chr(out, '\\');
	sb_chr(out, sep);
	sb_str(out, "|:vis 3");
	EMIT_SEP(out);
	sb_str(out, "ic 0");
	EMIT_SEP(out);
}

/* Open the "printf '%s%s%s\n' ..." that stages a body, writing its first two
 * arguments: the prologue plus the default register state (single quoted, so
 * every byte goes out verbatim), and the switches the shell contributes (a
 * double-quoted word). The printf is left open on its third argument, the body
 * proper. regs = 0 omits both halves, as a plain absolute script has no state
 * to switch; applied adds the $P2VI_PATCH register write, which only a driver
 * with compat blocks reads. osb is scratch, left empty. */
static void emit_body_head(sbuf *osb, int regs, int applied)
{
	sbuf_cut(osb, 0)
	emit_prologue(osb);
	if (regs)
		emit_reg_defaults(osb);
	fputs("printf '%s%s%s\\n' '", stdout);
	sq_write(osb->s, osb->s_n);
	fputs("'\\\n\"", stdout);
	sbuf_cut(osb, 0)
	if (regs)
		emit_reg_switches(osb, applied);
	sbuf_nul(osb)
	fputs(osb->s, stdout);
	fputs("\"\\\n'", stdout);
	sbuf_cut(osb, 0)
}

/* One "$VI -e" invocation: the printf body (|sc! prologue, per-file
 * b<k>/%ya 98/groups, vis 2, the writes, the 2q) staged into $P2VIF, then the
 * EXINIT $VI line naming the files in b<k> order. Buffer indices are the
 * position in active[], which is what vi opens. */
static void emit_vi_block(file_patch_t **active, int nactive)
{
	int regs = relative_mode || compat_mode;
	sbuf_smake(osb, SB_INIT)
	/* the three printf arguments sit on their own source lines, spliced by
	 * backslash-newline continuations, so the output is unchanged */
	emit_body_head(osb, regs, 0);
	if (relative_mode) {
		sb_str(osb, "fr 98");
		EMIT_SEP(osb);
	}
	for (int k = 0; k < nactive; k++)
		emit_file_body(osb, active[k], k,
			       relative_mode && !active[k]->is_new);
	emit_write_tail(osb, nactive, NULL);
	sq_write(osb->s, osb->s_n);
	fputs("' > \"$P2VIF\"\n" P2VI_VICALL " $VI -e", stdout);
	for (int k = 0; k < nactive; k++) {
		fputc(' ', stdout);
		sq_path(active[k]->path);
	}
	fputs(" \"$P2VIF\"\n", stdout);
	free(osb->s);
}

/* Global buffer index in uf[], the order the single $VI call opens the real
 * files in. Matched by path: the host's and a compat block's entries for one
 * file are distinct file_patch_t but one physical file, so one buffer. */
static int uf_index(file_patch_t **uf, int nuf, file_patch_t *fp)
{
	for (int i = 0; i < nuf; i++)
		if (!strcmp(uf[i]->path, fp->path))
			return i;
	return -1;
}

/* One section's edit body - no prologue, no register defaults, no identity
 * gate, no vis 2/w/2q tail: the per-file buffer select, its register cache and
 * its groups. Buffer indices are global (uf_index), since one $VI call opens
 * every file. The body is staged as its own buffer and run verbatim through %@,
 * which inserts the yanked bytes without rescanning them, so its top-level
 * separators stay raw however deep the call sits. */
static void emit_section_body(sbuf *out, file_patch_t **files, int nf,
			      file_patch_t **uf, int nuf)
{
	/* The driver expands this body through "2sc % : ?%@<reg>", so xexp is
	 * still '%' here - but the body's own %ya/%f> mean the all-lines range,
	 * not expansion. Reset it up front; the error sites re-enable it
	 * locally through emit_reg_call. */
	sb_str(out, "2sc!");
	EMIT_SEP(out);
	sb_str(out, "fr 98");
	EMIT_SEP(out);
	for (int k = 0; k < nf; k++)
		emit_file_body(out, files[k], uf_index(uf, nuf, files[k]),
			       !files[k]->is_new);
	/* A dangling separator from the last error check would be an empty
	 * command when %@ runs the body, which ex reports as unknown. */
	if (out->s_n > 0 && out->s[out->s_n - 1] == sep)
		out->s_n--;
}

/* The writes a compat block owns: the files no other section touches, which the
 * driver's write tail therefore leaves alone. A block's body only runs when
 * its identity gate fired, so a file the block's origin creates (lsp.c for
 * lsp.sh) is written when the origin is applied and never conjured - an empty
 * one - out of a buffer the block did not edit. Emitted inside the body, ahead
 * of the announce, so reaching the print still means everything landed. */
static void emit_section_writes(sbuf *out, file_patch_t **files, int nf,
				file_patch_t **uf, int nuf, const char *own)
{
	int k, gi, n = 0;
	for (k = 0; k < nf; k++) {
		gi = uf_index(uf, nuf, files[k]);
		if (gi < 0 || !own[gi])
			continue;
		/* Leave raw ex mode for the writes, the way the driver's write
		 * tail does: the '"file" 100L [w]' message is only syntax
		 * highlighted outside vis 3. Re-entered right after. */
		if (!n++) {
			EMIT_SEP(out);
			sb_str(out, "vis 2");
		}
		EMIT_SEP(out);
		sb_printf(out, "b%d", gi);
		EMIT_SEP(out);
		sb_str(out, "w");
	}
	if (n) {
		EMIT_SEP(out);
		sb_str(out, "vis 3");
	}
}

/* A compat block announces itself as the LAST command of its own body: the body
 * only runs when its identity gate fired, and reaching its end means every
 * edit applied - so the print is proof of application, not of intent. Silent on
 * a clean tree. No DBG switch hides it: it is the only outside evidence that a
 * compat block ran. */
static void emit_compat_announce(sbuf *out, int reg, char *origin)
{
	EMIT_SEP(out);
	sb_printf(out, "p compat %d applied: src=%s", reg,
		  origin ? origin : "");
}

/* Stage one section body as a shell here-string into "$P2VIF".<suf>, the file
 * the single $VI call opens as a buffer. The suffix is the block's own section
 * register - what "# Compat <reg>" above it says and what -E takes as its
 * selector - so the staged files name themselves; the host section, whose
 * register is the shared 97, keeps 0. It is a label, not an index: the
 * sections are staged, and opened, in run order, and that order is what the
 * driver's b<N> counts. It is a byte image of the body: the printf's own
 * newline stands in for a trailing one the body already has, the way the
 * dangling separator above is cut. Left in, that newline is an empty last
 * line of the staged buffer, which the yank hands the body's last command as
 * one more line of its argument - a body ending in an insert would append a
 * blank line to the file it edits. */
static void stage_section(sbuf *body, int suf)
{
	int n = body->s_n;
	if (n > 0 && body->s[n - 1] == '\n')
		n--;
	printf("printf '%%s\\n' '");
	sq_write(body->s, n);
	printf("' > \"$P2VIF\".%d\n", suf);
}

/* A section to run in the single call: its files, its register, and (for a
 * compat block) its index and the block it customizes from. */
typedef struct {
	file_patch_t **files;
	int nf;
	int reg;		/* register the driver %@-calls the body from */
	int secbuf;		/* global buffer index of the staged body */
	int suf;		/* "$P2VIF".<suf>: the section register, 0 = host */
	int blk;		/* compat block index (reg is base+blk); -1 host */
	char **src;		/* the block's src= basenames, parsed once (owned) */
	int nsrc;		/* how many; 0 can never fire */
	compat_block_t *cb;	/* NULL for the host section */
} section_t;

/* Do two sections edit any file in common (by path)? Blocks only stack over a
 * shared file, so the per-block subset test only considers later blocks that
 * touch the same file. */
static int sections_share_file(section_t *a, section_t *b)
{
	for (int i = 0; i < a->nf; i++)
		for (int j = 0; j < b->nf; j++)
			if (!strcmp(a->files[i]->path, b->files[j]->path))
				return 1;
	return 0;
}

/* Redefine REG_QF2 to the assert form, so an error site calling it quits 1 -
 * unless QF2=1 emptied REG_QF2A, which is the whole point of routing through
 * it: a run told to fall through must keep falling through however many times
 * a block rewrites the policy. The body is one command with no separator of
 * its own, so it needs no escape level and reads the same inside a ??-arm as
 * at the driver's top level.
 * The trailing separator stays raw either way: it ends this command in the
 * stream the driver's top level parses, and escaping it would fold every
 * following driver command - the block's buffer select and %ya included - into
 * the arm, to run only when the arm fires. */
static void emit_qf2_assert(sbuf *out)
{
	sb_printf(out, "%dreg ? %%@%d", REG_QF2, REG_QF2A);
	EMIT_SEP(out);
}

/* Redefine REG_QF2 to empty: error sites in the following body still report,
 * through REG_ERR2 -> REG_HDLR, but no longer abort. */
static void emit_qf2_clear(sbuf *out)
{
	sb_printf(out, "%dreg", REG_QF2);
	EMIT_SEP(out);
}

/* Host quit override, emitted once before the host body when any origin is
 * present: "211reg fr <ANY>:f> 1:?!? %@221:fr 98". A miss on the shared
 * any-origin flag (a clean tree, no origin in the applied set) is an error ??!
 * catches and asserts on,
 * exactly as a non-compat script does; a hit leaves it silent, so the host is
 * best-effort and falls through its own mismatches. It lives inside 211 and
 * re-runs per error site, so the flag is read with a register f>, never an
 * anchor. The assert is the shared 221 body, so QF2=1 relaxes this arm too.
 *
 * The trailing "fr 98" is load-bearing: ex_find's register redirection is the
 * global xfr, so reading the flag leaves every later search pointed at it, and
 * the next group would search the flag's "1" instead of the file cache. It sits
 * outside the arm, so the restore happens whether the arm asserts or not. */
static void emit_host_override(sbuf *out)
{
	sb_printf(out, "%dreg fr %d", REG_QF2, REG_FLAG_ANY);
	EMIT_ESCSEP(out);
	sb_str(out, "f> 1");
	EMIT_ESCSEP(out);
	sb_printf(out, "?" "?!? %%@%d", REG_QF2A);
	EMIT_ESCSEP(out);
	sb_str(out, "fr 98");
	EMIT_SEP(out);
}

/* The gate slots of every later block over the same file, ORed: the expression
 * a "??" branches on. Returns how many there were, 0 for none - a block with no
 * src= at all is left out, since it can never fire and so can never suppress.
 * A slot no gate ever recorded would abandon the whole lookup, not just its own
 * term, so an id only goes in when emit_compat_gates is sure to have written
 * it. */
static int sb_later_slots(sbuf *out, section_t *secs, int nsec, int i)
{
	int n = 0;
	for (int j = i + 1; j < nsec; j++) {
		if (!secs[j].cb || secs[j].blk < 0 || !secs[j].nsrc ||
		    !sections_share_file(&secs[i], &secs[j]))
			continue;
		if (n++)
			sb_chr(out, ';');
		sb_printf(out, "%d", secs[j].reg);
	}
	return n;
}

/* Block-head quit policy: a block asserts iff no later block over the same file
 * has a fired origin. Every gate's answer is already an anchor slot, recorded
 * once by emit_compat_gates under the block's own register number, so the test
 * is the ORed slots and nothing else - any present suppresses, none asserts. A
 * statically-last block emits no test at all and so asserts unconditionally,
 * restoring the assert the host override relaxed. */
static void emit_block_qf2(sbuf *out, section_t *secs, int nsec, int i)
{
	sbuf_smake(ids, 32)
	if (!sb_later_slots(ids, secs, nsec, i)) {
		free(ids->s);
		emit_qf2_assert(out);
		return;
	}
	sbuf_nul(ids)
	sb_str(out, ids->s);
	sb_str(out, "?" "?");
	emit_qf2_clear(out);
	sb_str(out, ids->s);
	sb_str(out, "?" "?!");
	emit_qf2_assert(out);
	free(ids->s);
}

/* The basename of a path: the text after the last '/'. */
static const char *base_name(const char *p)
{
	const char *s = strrchr(p, '/');
	return s ? s + 1 : p;
}

/* A compat block's origin label, as its src= fields' basenames: *out[i] is
 * one field, the count is returned. The label is "a.sh src=b.sh" (no
 * "src=" on the first), the shape compat_origin_label builds. */
static int compat_src_fields(compat_block_t *cb, char ***out)
{
	const char *s = cb->origin ? cb->origin : "";
	char **v = NULL;
	int n = 0, cap = 0;
	*out = NULL;
	while (s && *s) {
		const char *e = strstr(s, " src=");
		const char *end = e ? e : s + strlen(s);
		const char *b = s;
		for (const char *p = s; p < end; p++)
			if (*p == '/')
				b = p + 1;
		ARR_PUSH(v, n, cap)
		v[n++] = dup_n(b, end - b);
		s = e ? e + 5 : NULL;
	}
	*out = v;
	return n;
}

/* One src= member as a pattern for REG_APPLIED: "[ /]<base> ". The register is
 * space padded at both ends, so the delimiters always exist and a name matches
 * only whole - never as the tail of a longer one ("p.sh" inside "lsp.sh") -
 * while the '/' alternative accepts the "./x.sh" and "/abs/x.sh" spellings a
 * caller may have put in $P2VI_PATCH for the basename the field stores.
 * Every byte outside [A-Za-z0-9_-] goes in a one-character class, which is
 * literal to the regex and inert in the ex_arg escape layer both, so a '.' in
 * a script name needs no escape byte and cannot degrade into "any character".
 * A ']' or '^' in a script name is not representable this way and not
 * supported. */
static void sb_src_pat(sbuf *out, const char *base)
{
	sb_str(out, "[ /]");
	for (const char *p = base; *p; p++) {
		if (isalnum((unsigned char)*p) || *p == '_' || *p == '-')
			sb_chr(out, *p);
		else
			sb_printf(out, "[%c]", *p);
	}
	sb_chr(out, ' ');
}

/* The identity gates, decided in ex from the applied set alone - and, where one
 * fires, the section register it arms.
 *
 * REG_APPLIED already holds "<space> patch1.sh patch2.sh ... <space>", written
 * by the body head. Everything here is the editor's: "fr REG_APPLIED" points
 * searching at that register, each block searches it once per src= member
 * recording the outcome in an anchor slot, and the ANDed slots ("20,21??") run
 * the arm. The arm is what makes the block real: it yanks the block's staged
 * body into the block's own register, so a gate that misses simply leaves that
 * register undefined and the block's call later finds nothing to run. No flag
 * is written and none is read - the body is the flag. REG_FLAG_ANY rides along
 * in the same arm, so the any-origin answer the host override reads needs no
 * second pass. A block with no src= field at all has no arm, so it can never
 * fire.
 *
 * The buffer select the yank needs stays outside the arm, unconditional. That
 * is not a style choice: remap_bufnums only rewrites a command that is exactly
 * "b<N>", and the replay path needs every buffer number rewritten, so a "b<N>"
 * buried in an argument would silently keep the emitting run's numbering. The
 * cost is a buffer switch a missing block does not use.
 *
 * The trailing "<reg>??" records whether the arm fired, under the block's own
 * register number, for the quit policy of any earlier block over the same file
 * to read. It must sit here, against the arm, and not at the call: by then the
 * last command's status is something else entirely.
 *
 * The arm holds two commands, joined by an escaped separator: ex_arg unescapes
 * it, so the ex_exec the arm runs sees a chain. Its "%ya" is safe unexpanded
 * only because the driver prologue's "|sc!" left xexp inert - inside an arm the
 * text is an argument, and with xexp live that % would expand to a buffer path.
 * A gate that misses leaves xpret set and the "??" returns xuerr, which with
 * the default xerr is neither printed nor fatal - the way any unfired arm reads.
 *
 * The closing "fr 98" hands searching back to the file cache: every section
 * body sets it again itself, but nothing should have to rely on that. */
static void emit_compat_gates(sbuf *out, section_t *secs, int nsec)
{
	sb_printf(out, "%dreg 0", REG_FLAG_ANY);
	EMIT_SEP(out);
	sb_printf(out, "fr %d", REG_APPLIED);
	EMIT_SEP(out);
	for (int i = 0; i < nsec; i++) {
		int nf;
		if (!secs[i].cb)
			continue;
		nf = secs[i].nsrc;
		/* one source line per block: its scans, its arm, its answer */
		EMIT_LB(out);
		EMIT_SEP(out);
		for (int k = 0; k < nf; k++) {
			sb_str(out, "f> ");
			sb_src_pat(out, secs[i].src[k]);
			EMIT_SEP(out);
			sb_printf(out, "%d?" "?", SRC_SLOT_BASE + k);
			EMIT_SEP(out);
		}
		if (!nf)
			continue;
		sb_printf(out, "b%d", secs[i].secbuf);
		EMIT_SEP(out);
		/* the scans ANDed: the block arms only if every origin is in */
		for (int k = 0; k < nf; k++)
			sb_printf(out, k ? ",%d" : "%d", SRC_SLOT_BASE + k);
		sb_printf(out, "?" "? %%ya %d", secs[i].reg);
		EMIT_ESCSEP(out);
		sb_printf(out, "%dreg 1", REG_FLAG_ANY);
		EMIT_SEP(out);
		sb_printf(out, "%d?" "?", secs[i].reg);
		EMIT_SEP(out);
	}
	EMIT_LB(out);
	EMIT_SEP(out);
	sb_str(out, "fr 98");
	EMIT_SEP(out);
}

/* Call half: "%@" the section out of its register. The host section is yanked
 * into 97 right here, since without a compat block there is no gate pass to do
 * it in; a compat block was armed by its gate, if its gate fired, and the call
 * is unconditional either way - an undefined register expands to nothing and
 * "?" with an empty argument runs nothing. Bracketed with the "2sc %" / "2sc!"
 * expansion window, since the driver prologue's |sc! leaves xexp inert. */
static void emit_driver_call(sbuf *out, section_t *secs, int nsec, int i,
			     file_patch_t **uf, int nuf, const char *own)
{
	section_t *s = &secs[i];
	/* Rewind every real file this section touches: an earlier block
	 * leaves the cursor deep in the buffer, and the body's
	 * relative searches key off the current line. The rewind is
	 * unconditional, so a block whose origin is absent still rewinds the
	 * buffer of the file that origin creates - empty, so line 1 of it is
	 * "invalid range". Only a section holding such a file pays for
	 * "err 0": a rewind over a file the host patches cannot fail. */
	int risky = 0;
	for (int k = 0; k < s->nf; k++) {
		int gi = uf_index(uf, nuf, s->files[k]);
		risky |= gi >= 0 && own[gi];
	}
	if (risky) {
		sb_str(out, "err 0");
		EMIT_SEP(out);
	}
	for (int k = 0; k < s->nf; k++) {
		sb_printf(out, "b%d", uf_index(uf, nuf, s->files[k]));
		EMIT_SEP(out);
		sb_str(out, "1");
		EMIT_SEP(out);
	}
	if (risky) {
		sb_str(out, "err 1");
		EMIT_SEP(out);
	}
	/* Set this block's quit policy before its body runs: assert if it is the
	 * last firing block over its file, suppress otherwise. */
	if (s->cb)
		emit_block_qf2(out, secs, nsec, i);
	/* Readability line break where the setup ends and the dispatch begins:
	 * everything above is this section's rewinds and quit policy, everything
	 * below its call. */
	EMIT_LB(out);
	EMIT_SEP(out);
	if (!s->cb) {
		sb_printf(out, "b%d", s->secbuf);
		EMIT_SEP(out);
		sb_printf(out, "%%ya %d", s->reg);
		EMIT_SEP(out);
	}
	sb_str(out, "2sc %");
	EMIT_SEP(out);
	sb_printf(out, "? %%@%d", s->reg);
	EMIT_SEP(out);
	sb_str(out, "2sc!");
	EMIT_SEP(out);
}

/* The whole patch as a single $VI call: the real files as b0..bN-1, one staged
 * buffer per section (host, then each compat block), and a driver buffer EXINIT
 * yanks into register 97 and runs. The driver defines the state registers once,
 * %@-calls each section in application order, then writes every real file and
 * quits. An identity gate that misses only skips its block's call - nothing
 * quits the shared process, so later sections still run. */
static void emit_one_call(file_patch_t **active, int nactive)
{
	section_t *secs = emalloc((ncompat + 1) * sizeof(*secs));
	int nsec = 0, nwrite;
	file_patch_t **uf;
	char *own;		/* uf slots a compat body writes itself */
	int nuf = 0;

	/* Sections in run order: host, then every compat block (all post). */
	if (nactive > 0) {
		secs[nsec].files = active;
		secs[nsec].nf = nactive;
		secs[nsec].reg = P2VI_REG;
		secs[nsec].blk = -1;
		secs[nsec].src = NULL;
		secs[nsec].nsrc = 0;
		secs[nsec].cb = NULL;
		nsec++;
	}
	int ncsec = 0;
	for (int c = 0; c < ncompat; c++) {
		compat_block_t *cb = &compat_blocks[c];
		int nca;
		file_patch_t **ca = block_files(cb, &nca);
		if (!nca) {
			free(ca);
			continue;
		}
		secs[nsec].files = ca;
		secs[nsec].nf = nca;
		/* the register is the block's own index, so a skipped block
		 * does not shift the registers the rest read */
		secs[nsec].blk = c;
		secs[nsec].reg = REG_SEC_BASE + c;
		/* the label is parsed here and nowhere else: the gate block,
		 * the "# Compat" comment and the quit policy all read it */
		secs[nsec].nsrc = compat_src_fields(cb, &secs[nsec].src);
		secs[nsec].cb = cb;
		nsec++;
		ncsec++;
	}

	/* Buffer order follows the sections, not files[]: a script's stored
	 * compat regions sit before its host patch, so a regen parses them in
	 * the other order than the run that derived them did, and a
	 * files[]-ordered b<N> would renumber every buffer across it. */
	uf = emalloc((nfiles + ncompat + 1) * sizeof(*uf));
	for (int i = 0; i < nsec; i++)
		for (int j = 0; j < secs[i].nf; j++)
			if (uf_index(uf, nuf, secs[i].files[j]) < 0)
				uf[nuf++] = secs[i].files[j];
	nwrite = nuf;

	/* Files no host section edits: their only writer is the compat block
	 * that names them, and it writes them from inside its own gated body -
	 * so an origin's new file (its whole point is that it does not exist
	 * yet) is not created empty by a run the origin is missing from. One
	 * pass collects each slot's writers (1 = a block, 2 = the host section),
	 * and a slot no host wrote is the one its block owns. */
	own = ecalloc(nuf + 1, 1);
	for (int i = 0; i < nsec; i++)
		for (int j = 0; j < secs[i].nf; j++) {
			int gi = uf_index(uf, nuf, secs[i].files[j]);
			if (gi >= 0)
				own[gi] |= secs[i].cb ? 1 : 2;
		}
	for (int i = 0; i < nuf; i++)
		own[i] = own[i] == 1;

	fputs("# Body too large for EXINIT/argv: stage it in a file\n"
	      "( : > /tmp/p2vi.$$.d ) 2>/dev/null && P2VIF=/tmp/p2vi.$$ || P2VIF=./p2vi.$$\n"
	      "trap 'rm -f \"$P2VIF\".*' EXIT\n\n", stdout);

	/* the driver references these before the bodies are staged */
	for (int i = 0; i < nsec; i++) {
		secs[i].secbuf = nuf + i;
		secs[i].suf = secs[i].cb ? secs[i].reg : 0;
	}

	/* The driver (".d") is staged first: prologue + register defaults,
	 * shell switches, then orchestration and the final writes. */
	sbuf_smake(osb, SB_INIT)
	emit_body_head(osb, 1, ncsec > 0);
	if (ncsec > 0) {
		/* the driver decides every identity gate itself, out of the
		 * applied set the head just put in REG_APPLIED */
		emit_compat_gates(osb, secs, nsec);
		/* then, before any body: with an origin present (the any flag
		 * set) the host override relaxes its own quit chain, so the
		 * compat blocks can repair its misses; on a clean tree 211
		 * keeps its default assert */
		emit_host_override(osb);
	}
	for (int i = 0; i < nsec; i++)
		emit_driver_call(osb, secs, nsec, i, uf, nuf, own);
	emit_write_tail(osb, nwrite, own);
	sq_write(osb->s, osb->s_n);
	putchar('\'');
	printf(" > \"$P2VIF\".d\n");
	free(osb->s);

	/* Stage each section body after the driver. */
	for (int i = 0; i < nsec; i++) {
		section_t *s = &secs[i];
		int sv_rel = 0;
		if (s->cb) {
			/* the block's identity gate, spelled out for a reader:
			 * the register emit_compat_gates arms with this body,
			 * and every src= that has to be in the applied set for
			 * it to be armed at all. Each origin carries the "src="
			 * the label leaves off its first, so the fields read
			 * alike and grep alike. Every block is post, so nothing
			 * says so. */
			printf("# Compat %d", s->reg);
			for (int k = 0; k < s->nsrc; k++)
				printf(" src=%s", s->src[k]);
			printf("\n");
			compat_win_enter(&sv_rel);
		}
		sbuf_smake(bsb, SB_INIT)
		emit_section_body(bsb, s->files, s->nf, uf, nuf);
		if (s->cb) {
			emit_section_writes(bsb, s->files, s->nf, uf, nuf, own);
			emit_compat_announce(bsb, s->suf, s->cb->origin);
		}
		sbuf_nul(bsb)
		stage_section(bsb, s->suf);
		free(bsb->s);
		if (s->cb)
			compat_win_leave(sv_rel);
	}

	/* real files, section bodies, driver last - so it is current at EXINIT
	 * and "%ya 97" yanks it */
	fputs(P2VI_VICALL " $VI -e", stdout);
	for (int i = 0; i < nuf; i++) {
		fputc(' ', stdout);
		sq_path(uf[i]->path);
	}
	for (int i = 0; i < nsec; i++)
		printf(" \"$P2VIF\".%d", secs[i].suf);
	printf(" \"$P2VIF\".d\n");

	for (int i = 0; i < nsec; i++) {
		free_lines(secs[i].src, secs[i].nsrc);
		if (secs[i].cb)
			free(secs[i].files);
	}
	free(secs);
	free(uf);
	free(own);
}

/* Every compat block as a terminator-fenced tail region after exit 0 and before
 * the host === PATCH2VI PATCH === (which stays last, to EOF). One region per
 * compat patch, self-contained - its whole unified diff - so a regen carries it
 * over without re-running the origin. === COMPAT PATCH === is that diff and
 * nothing else, stored verbatim, so a -C second positional comes back out as
 * the patch its author handed in. The sub-section closes with === END === like
 * the host's, so the reader reaches === END COMPAT === with no section open.
 *
 * A block's identity gate is the applied set, so nothing of it is stored here:
 * it is derived from $P2VI_PATCH at run time. */
static void emit_compat_storage(void)
{
	for (int c = 0; c < ncompat; c++) {
		compat_block_t *cb = &compat_blocks[c];
		printf("=== PATCH2VI COMPAT %d src=%s ===\n",
		       REG_SEC_BASE + c, cb->origin ? cb->origin : "");
		printf("=== COMPAT PATCH ===\n");
		for (int i = 0; i < cb->raw.n; i++)
			fputs(cb->raw.v[i], stdout);
		printf("%s\n", end_tag_wr);
		printf("=== END COMPAT ===\n");
	}
}

/* set by "--- /dev/null", consumed by the next "+++" */
static int pending_is_new;
/* "---" path (pre-patch original), consumed by the next "+++" */
static char *pending_orig_path;

static void new_file(const char *path)
{
	ARR_PUSH(files, nfiles, files_cap)
	files[nfiles].path = uc_dup(path);
	files[nfiles].is_new = pending_is_new;
	files[nfiles].orig_path = pending_orig_path;
	pending_is_new = 0;
	pending_orig_path = NULL;
	nfiles++;
	/* path appears in the FAIL <path>:<line> error message inside EXINIT */
	mark_bytes_used(path);
}

/* Original-line span of the @@ hunk currently being parsed (0 = none yet). */
static int cur_hunk_lo, cur_hunk_hi;

static void add_op(int type, int oline, const char *text)
{
	if (nfiles == 0)
		return;
	file_patch_t *fp = &files[nfiles - 1];
	ARR_PUSH(fp->ops, fp->nops, fp->ops_cap)
	fp->ops[fp->nops].type = type;
	fp->ops[fp->nops].oline = oline;
	fp->ops[fp->nops].text = text ? uc_dup(text) : NULL;
	fp->ops[fp->nops].hunk_lo = cur_hunk_lo;
	fp->ops[fp->nops].hunk_hi = cur_hunk_hi;
	fp->nops++;

	/* the text itself is patch content: its bytes join the census */
	if (text)
		mark_bytes_used(text);
}

/*
 * -e: run a generated script through the embedded editor, no shell involved.
 * The grammar is closed and self-generated - header assignments, one printf'd
 * ex body per editor invocation, the "$VI -e <files> $P2VIF" line that runs it -
 * so it is parsed exactly and anything outside it is an error, not a
 * best-effort guess. Each block gets its own editor lifetime, mirroring the
 * separate $VI process the shell would spawn.
 */

static int exec_mode;		/* -e: execute the input script */
static const char *exec_script;	/* its path, i.e. the script's $0 */

/* Shell variables assigned by the script header, looked up before the
 * environment - so an assignment shadows an inherited value while the header's
 * own conditionals still test the inherited one. */
typedef struct {
	char *name;
	char *val;
} shvar_t;

static shvar_t *shvars;
static int nshvars, shvars_cap;

static const char *sh_get(const char *name)
{
	for (int i = 0; i < nshvars; i++)
		if (!strcmp(shvars[i].name, name))
			return shvars[i].val;
	return getenv(name);
}

/* Header assignments belong to one script: replaying two (-C) must not let the
 * first's shadow the environment while the second's conditionals are read. */
static void sh_reset(void)
{
	for (int i = 0; i < nshvars; i++) {
		free(shvars[i].name);
		free(shvars[i].val);
	}
	nshvars = 0;
}

static void sh_set(const char *name, const char *val)
{
	for (int i = 0; i < nshvars; i++)
		if (!strcmp(shvars[i].name, name)) {
			free(shvars[i].val);
			shvars[i].val = uc_dup(val);
			return;
		}
	ARR_PUSH(shvars, nshvars, shvars_cap)
	shvars[nshvars].name = uc_dup(name);
	shvars[nshvars].val = uc_dup(val);
	nshvars++;
}

static int sh_err(const char *what, const char *s)
{
	fprintf(stderr, "-e: unsupported %s: %s\n", what, s);
	return -1;
}

/* The [A-Za-z0-9_] run at *s as a fresh string, advancing *s past it. */
static char *sh_name(const char **s)
{
	sbuf_smake(sb, 32)
	while (isalnum((unsigned char)**s) || **s == '_')
		sbuf_chr(sb, *(*s)++)
	sbufn_ret(sb, sb->s)
}

/* One double-quoted shell word: ${VAR}, ${VAR:-default} and ${VAR:+alternate}
 * (both nestable), $VAR, $0, $(printf '\NNN') and the escapes that survive
 * double quotes. Everything else is literal. */
static int sh_expand(const char *s, sbuf *out)
{
	char *name;
	int j;
	while (*s) {
		if (*s == '\\' && s[1] == '\n') {	/* line continuation */
			s += 2;
			continue;
		}
		if (*s == '\\' && (s[1] == '$' || s[1] == '"' || s[1] == '\\'
				   || s[1] == '`')) {
			sbuf_chr(out, s[1])
			s += 2;
			continue;
		}
		if (*s != '$') {
			sbuf_chr(out, *s++)
			continue;
		}
		s++;
		if (*s == '(') {	/* $(printf 'escapes') */
			if (strncmp(s, "(printf '", 9))
				return sh_err("substitution", s);
			for (s += 9; *s && *s != '\''; ) {
				if (*s != '\\') {
					sbuf_chr(out, *s++)
					continue;
				}
				s++;
				if (*s >= '0' && *s <= '7') {
					int v = 0;
					for (j = 0; j < 3 && *s >= '0'
					     && *s <= '7'; j++, s++)
						v = v * 8 + (*s - '0');
					sbuf_chr(out, v)
					continue;
				}
				if (*s == 'n')
					sbuf_chr(out, '\n')
				else if (*s == 't')
					sbuf_chr(out, '\t')
				else if (*s == '\\' || *s == '\'')
					sbuf_chr(out, *s)
				else
					return sh_err("printf escape", s - 1);
				s++;
			}
			if (strncmp(s, "')", 2))
				return sh_err("substitution", s);
			s += 2;
			continue;
		}
		if (*s == '0') {	/* $0: the script itself */
			s++;
			sbuf_str(out, exec_script ? exec_script : "patch2vi")
			continue;
		}
		if (*s == '{') {
			const char *val;
			s++;
			name = sh_name(&s);
			j = *name != '\0';
			val = j ? sh_get(name) : NULL;
			free(name);
			if (!j)
				return sh_err("expansion", s);
			if (*s == '}') {
				s++;
				if (val)
					sbuf_str(out, val)
				continue;
			}
			if (s[0] != ':' || (s[1] != '-' && s[1] != '+'))
				return sh_err("expansion", s);
			int alt = s[1] == '+';
			s += 2;
			/* the word runs to the matching brace and is
			 * skipped whether or not it is the one used */
			const char *b = s;
			for (int depth = 1; depth; ) {
				if (!*s)
					return sh_err("expansion", b);
				else if (s[0] == '$' && s[1] == '{') {
					depth++;
					s += 2;
				} else if (*s == '}') {
					if (!--depth)
						break;
					s++;
				} else
					s++;
			}
			/* ":-" takes the word when unset, ":+" when set */
			if ((val && *val ? 1 : 0) != alt) {
				if (!alt)
					sbuf_str(out, val)
			} else {
				sbuf_smake(def, SB_INIT)
				sbuf_mem(def, b, s - b)
				sbuf_nul(def)
				j = sh_expand(def->s, out);
				free(def->s);
				if (j < 0)
					return -1;
			}
			s++;	/* the closing brace */
			continue;
		}
		if (isalpha((unsigned char)*s) || *s == '_') {
			const char *val;
			name = sh_name(&s);
			if ((val = sh_get(name)))
				sbuf_str(out, val)
			free(name);
			continue;
		}
		return sh_err("expansion", s - 1);
	}
	return 0;
}

/* NAME=value, with value optionally double quoted */
static int sh_assign(const char *s)
{
	char *name = sh_name(&s);
	int ret;
	if (!*name || *s != '=') {
		free(name);
		return sh_err("assignment", s);
	}
	s++;
	sbuf_smake(val, SB_INIT)
	if (*s == '"') {
		int n = strlen(s);
		if (n < 2 || s[n - 1] != '"') {
			free(val->s);
			return sh_err("assignment", s);
		}
		sbuf_mem(val, s + 1, n - 2)
		sbuf_nul(val)
		s = val->s;
	}
	sbuf_smake(out, SB_INIT)
	if (!(ret = sh_expand(s, out))) {
		sbuf_nul(out)
		sh_set(name, out->s);
	}
	free(out->s);
	free(val->s);
	return ret;
}

typedef struct {
	char **paths;	/* real files, in $VI argument order */
	int npaths;
	char *body;	/* the printf'd ex command body (driver body, new shape) */
	int sep;	/* the script's separator byte, its commands' delimiter */
	char **sects;	/* new shape: one staged section body per buffer, in index
			 * order; NULL/0 for the old single-body shape */
	int nsects;
} p2vi_block_t;

/* One editor lifetime: the real files as b0..bN-1, then one in-RAM buffer per
 * staged section body, then the driver body. EXINIT only exists to lift the
 * body out of the buffer the shell had to pass it in; -e holds it already, so
 * it fills the register the body may recurse through and runs the chain itself.
 * The driver does its own "b<k>:%ya <reg>:...%@<reg>" per section, so the
 * section bodies must be resident before it runs - at exactly the emitter's
 * uf-count + section index. */
static int run_body(p2vi_block_t *blk)
{
	int st;
	if (ed_init(0) < 0)
		return -1;
	xvis |= 2;
	xbufsalloc = MAX(blk->npaths + blk->nsects + 1, MAX(64, xbufsalloc));
	ec_setbufsmax(NULL, NULL, "");
	for (int i = 0; i < blk->npaths; i++) {
		xmpt = 0;
		ec_edit("", "e", blk->paths[i]);
	}
	for (int i = 0; i < blk->nsects; i++) {
		char sname[32];
		snprintf(sname, sizeof(sname), "*p2vi-sec-%d*", i);
		xmpt = 0;
		ed_loadbuf(sname, blk->sects[i]);
	}
	xmpt = 0;
	xvis &= ~4;
	ex_regput(P2VI_REG, blk->body, 0);
	ex_exec(blk->body);
	if (!xquit)
		ex();
	st = ed_done();
	ed_free();
	return st;
}

static void free_block(p2vi_block_t *blk)
{
	for (int i = 0; i < blk->npaths; i++)
		free(blk->paths[i]);
	free(blk->paths);
	for (int i = 0; i < blk->nsects; i++)
		free(blk->sects[i]);
	free(blk->sects);
	free(blk->body);
	memset(blk, 0, sizeof(*blk));
}

/* The script's separator byte, read where ex learns it - the body's own
 * "|sc! <esc><sep>|" prologue, which holds the bytes as they are with a dynamic
 * escape and a doubled one with the default backslash. Per block, since a
 * replay may span two scripts and each body keeps its own. */
static int body_sepbyte(const char *body)
{
	const char *p = body, *e;
	if (!strncmp(p, "|sc! ", 5) && (e = strchr(p + 5, '|'))) {
		p += 5;
		if (e - p == 2)
			return (unsigned char)p[1];
		if (e - p == 3 && p[0] == '\\' && p[1] == '\\')
			return (unsigned char)p[2];
	}
	fprintf(stderr, "replay: body has no specials prologue\n");
	return -1;
}

/* EXINIT='<init>' $VI -e 'file' ... "$P2VIF": the init is the fixed one the
 * emitter writes and -e supplies its effect itself, so it is only checked */
static int parse_vi_call(const char *s, p2vi_block_t *blk)
{
	int closed;
	sbuf_smake(w, SB_INIT)
	if (strncmp(s, P2VI_VICALL, strlen(P2VI_VICALL))) {
		free(w->s);
		return sh_err("vi call", s);
	}
	s += strlen(P2VI_VICALL);
	if (strncmp(s, " $VI -e", 7)) {
		free(w->s);
		return sh_err("vi call", s);
	}
	for (s += 7; *s; ) {
		if (*s == ' ') {
			s++;
			continue;
		}
		if (!strncmp(s, "\"$P2VIF\"", 8)) {
			s += 8;
			/* new shape names section/driver buffers as "$P2VIF".<sfx>;
			 * the section bodies are loaded from the staged printfs, so
			 * here the suffix is only skipped, not turned into a path */
			if (*s == '.')
				while (*s && *s != ' ')
					s++;
			continue;
		}
		/* one quoted word, as sq_path wrote it: a quote closes it and a
		 * '\'' reopens it around an embedded quote */
		if (*s != '\'') {
			free(w->s);
			return sh_err("vi call", s);
		}
		sbuf_cut(w, 0)
		closed = 0;
		for (s++; *s && !closed; ) {
			if (*s == '\'' && s[1] == '\\' && s[2] == '\'' &&
			    s[3] == '\'') {
				sbuf_chr(w, '\'')
				s += 4;
			} else if (*s == '\'') {
				s++;
				closed = 1;
			} else
				sbuf_chr(w, *s++)
		}
		if (!closed) {
			free(w->s);
			return sh_err("vi call", s);
		}
		sbuf_nul(w)
		blk->paths = erealloc(blk->paths,
				      (blk->npaths + 1) * sizeof(char *));
		blk->paths[blk->npaths++] = uc_dup(w->s);
	}
	free(w->s);
	return 0;
}

/* One shell word: adjacent single-quoted, double-quoted and unquoted runs
 * concatenate into it. A single-quoted run is verbatim, a double-quoted one
 * goes through sh_expand. */
static int sh_word(const char **sp, sbuf *out)
{
	const char *s = *sp, *e;
	int ret = 0;
	while (*s && *s != ' ' && *s != '\n') {
		if (*s == '\'') {
			if (!(e = strchr(++s, '\'')))
				return sh_err("quoting", s);
			sbuf_mem(out, s, e - s)
			s = e + 1;
			continue;
		}
		if (*s != '"') {
			/* backslash-newline is a line continuation: both bytes
			 * vanish, so a word wrapped across lines stays one word
			 * (this is what splices the three printf arguments) */
			if (*s == '\\' && s[1] == '\n') {
				s += 2;
				continue;
			}
			if (*s == '\\' && s[1])
				s++;
			sbuf_chr(out, *s++)
			continue;
		}
		for (e = ++s; *e && *e != '"'; e++)
			e += *e == '\\' && e[1];
		if (!*e)
			return sh_err("quoting", s);
		sbuf_smake(dq, SB_INIT)
		sbuf_mem(dq, s, e - s)
		sbuf_nul(dq)
		ret = sh_expand(dq->s, out);
		free(dq->s);
		if (ret < 0)
			return ret;
		s = e + 1;
	}
	*sp = s;
	return 0;
}

/* The staged body: "printf <format> <word>..." with a format that only
 * concatenates its arguments and ends the line. sh writes at most one word, and
 * only ever whole commands, so nothing here changes how the rest parses. */
static int sh_printf_body(const char *s, sbuf *out)
{
	int ret;
	sbuf_smake(fmt, SB_INIT)
	if (strncmp(s, "printf ", 7)) {
		free(fmt->s);
		return sh_err("body", s);
	}
	s += 7;
	if (!(ret = sh_word(&s, fmt))) {
		const char *p = fmt->s;
		sbuf_nul(fmt)
		while (p[0] == '%' && p[1] == 's')
			p += 2;
		if (p == fmt->s || strcmp(p, "\\n"))
			ret = sh_err("body format", fmt->s);
	}
	free(fmt->s);
	while (ret >= 0 && *s == ' ') {
		while (*s == ' ')
			s++;
		ret = sh_word(&s, out);
	}
	if (ret >= 0)
		sbuf_chr(out, '\n')
	return ret;
}

/* Expand one raw "printf <fmt> <word>..." command into its emitted text. */
static int expand_body(const char *raw, char **out)
{
	int ret;
	sbuf_smake(exp, SB_INIT)
	ret = sh_printf_body(raw, exp);
	sbuf_nul(exp)
	if (ret >= 0)
		*out = uc_dup(exp->s);
	free(exp->s);
	return ret;
}

/* Staged bodies gathered between two "$VI" calls, keyed by the "$P2VIF" suffix
 * they redirect into: "" single-body, "d" driver, "0","1",... sections. */
typedef struct {
	char **raw;	/* the raw printf command, one per staged body */
	char **suf;	/* its "$P2VIF" suffix */
	int n, cap;
} pend_t;

static void pend_push(pend_t *p, const char *raw, const char *suf)
{
	/* one capacity for two arrays: the saved copy gives the second ARR_PUSH
	 * the same pre-growth value, so both grow in step */
	int cap = p->cap;
	ARR_PUSH(p->raw, p->n, p->cap)
	ARR_PUSH(p->suf, p->n, cap)
	p->raw[p->n] = uc_dup(raw);
	p->suf[p->n++] = uc_dup(suf);
}

static void pend_clear(pend_t *p)
{
	for (int i = 0; i < p->n; i++) {
		free(p->raw[i]);
		free(p->suf[i]);
	}
	free(p->raw);
	free(p->suf);
	memset(p, 0, sizeof(*p));
}

/* The gathered bodies as one block: either the single "" body, or the "d"
 * driver plus the section bodies. The sections keep the order they were staged
 * in, which is the order the $VI call opens them in and so the order the
 * driver's b<N> counts; their "$P2VIF" suffix is a label (the block's section
 * register, or 0 for the host) and is not read as an index. Scripts emitted
 * when it was one - ".0", ".1", ".2" - parse the same, being in that order. */
static int pend_finish(pend_t *p, p2vi_block_t *blk)
{
	int drv = -1, old = -1, nsec = 0, ret = 0;
	for (int i = 0; i < p->n; i++) {
		if (!p->suf[i][0])
			old = i;
		else if (!strcmp(p->suf[i], "d"))
			drv = i;
		else
			nsec++;
	}
	if (old >= 0) {
		ret = expand_body(p->raw[old], &blk->body);
	} else if (drv >= 0) {
		blk->sects = ecalloc(nsec + 1, sizeof(char *));
		blk->nsects = nsec;
		nsec = 0;
		for (int i = 0; ret >= 0 && i < p->n; i++) {
			if (!p->suf[i][0] || !strcmp(p->suf[i], "d"))
				continue;
			ret = expand_body(p->raw[i], &blk->sects[nsec++]);
		}
		if (ret >= 0)
			ret = expand_body(p->raw[drv], &blk->body);
	} else
		ret = sh_err("body", "no staged body before $VI call");
	if (ret >= 0)
		blk->sep = body_sepbyte(blk->body);
	return ret;
}

/* The applied set of the script currently being parsed (-e from the earlier
 * scripts on the command line, replay from the earlier scripts in the chain):
 * basenames, so a stored src=a/b.sh field matches the basename a/b.sh puts in
 * $P2VI_PATCH. This is what -e publishes as $P2VI_PATCH for the driver. */
static char **cur_applied;
static int ncur_applied, cur_applied_cap;

static void cur_applied_clear(void)
{
	for (int i = 0; i < ncur_applied; i++)
		free(cur_applied[i]);
	ncur_applied = 0;
}

/* Replace the applied set with the basenames of paths[0..n-1], on top of
 * whatever $P2VI_PATCH the environment carries (the shell chain's own channel
 * - so -E and -C replays over a tree that already carries origins can assert
 * them the way the shell would have inherited them). Both are basenames. */
static void cur_applied_set(const char **paths, int n)
{
	cur_applied_clear();
	const char *env = getenv("P2VI_PATCH");
	if (env && *env) {
		char *cpy = uc_dup(env), *save = NULL;
		for (char *tok = strtok_r(cpy, " ", &save); tok;
		     tok = strtok_r(NULL, " ", &save)) {
			if (!*tok)
				continue;
			ARR_PUSH(cur_applied, ncur_applied, cur_applied_cap)
			cur_applied[ncur_applied++] = uc_dup(base_name(tok));
		}
		free(cpy);
	}
	for (int i = 0; i < n; i++) {
		const char *b = base_name(paths[i]);
		ARR_PUSH(cur_applied, ncur_applied, cur_applied_cap)
		cur_applied[ncur_applied++] = uc_dup(b);
	}
}

/* Free the applied-set paths once, at the end of a run. */
static void cur_applied_free(void)
{
	cur_applied_clear();
	free(cur_applied);
	cur_applied = NULL;
	cur_applied_cap = 0;
}

/* The applied set as the one space separated word $P2VI_PATCH holds in the
 * shell chain. The staged body head writes that variable into REG_APPLIED and
 * every compat block is decided from it, so handing -e the same string is the
 * whole of what -e has to do differently. */
static char *cur_applied_word(void)
{
	sbuf_smake(sb, SB_INIT)
	for (int i = 0; i < ncur_applied; i++) {
		if (i)
			sbuf_chr(sb, ' ')
		sbuf_str(sb, cur_applied[i])
	}
	sbufn_ret(sb, sb->s)
}

/* The script's executable region (everything before "exit 0") as one block per
 * editor invocation. Parsing is separate from running because -e gives each
 * block its own lifetime while the compat session replays them all in one, and
 * that needs the whole list up front. */
static int parse_p2vi_script(FILE *in, p2vi_block_t **blks, int *nblks)
{
	const char *body_end = " > \"$P2VIF\"";
	p2vi_block_t blk = {0};
	pend_t pend = {0};
	int skip = 0, in_body = 0, ret = 0, j;
	char *line;
	sbuf_smake(lb, SB_INIT)
	sbuf_smake(body, SB_INIT)
	while (ret >= 0 && (line = read_line(in, lb))) {
		char *seg = line;
		chomp(line);
		if (!in_body && !strncmp(line, "printf '", 8)) {
			sbuf_cut(body, 0)
			in_body = 1;
		}
		if (in_body) {
			/* the printf ends where it is redirected; its words
			 * span lines, so they are gathered raw and read once
			 * the whole command is in hand. The redirect's suffix
			 * ("" / ".d" / ".0"...) routes it in pend_finish. */
			int n = strlen(seg), el = strlen(body_end);
			const char *suf = NULL;
			if (n >= el && !strcmp(seg + n - el, body_end)) {
				seg[n - el] = '\0';
				suf = "";
			} else for (int i = n - el; i >= 0; i--) {
				if (strncmp(seg + i, body_end, el) || seg[i+el] != '.')
					continue;
				suf = seg + i + el + 1;
				seg[i] = '\0';
				break;
			}
			sbuf_str(body, seg)
			if (!suf) {
				sbuf_chr(body, '\n')
				continue;
			}
			in_body = 0;
			sbuf_nul(body)
			pend_push(&pend, body->s, suf);
			sbuf_cut(body, 0)
			continue;
		}
		if (!strcmp(line, "exit 0"))
			break;
		if (skip) {
			if (!strcmp(line, "fi"))
				skip--;
			else if (!strncmp(line, "if ", 3))
				skip++;
			continue;
		}
		if (!line[0] || line[0] == '#')
			continue;
		if (!strncmp(line, "if ", 3)) {
			skip++;
			continue;
		}
		/* the temp file the body would be staged in, and its trap:
		 * -e keeps the body in RAM, so both are moot */
		if (!strncmp(line, "( : > ", 6) || !strncmp(line, "trap ", 5))
			continue;
		if (!strncmp(line, "EXINIT=", 7)) {
			/* the applied set the shell chain would have inherited,
			 * published before the bodies expand so the head's
			 * "229reg  $P2VI_PATCH " resolves to it and the identity
			 * gates decide this call's blocks as a chain would */
			char *ap = cur_applied_word();
			sh_set("P2VI_PATCH", ap);
			free(ap);
			if ((ret = parse_vi_call(line, &blk)) < 0)
				break;
			if (!(ret = pend_finish(&pend, &blk))) {
				*blks = erealloc(*blks, (*nblks + 1)
						 * sizeof(**blks));
				(*blks)[(*nblks)++] = blk;
				memset(&blk, 0, sizeof(blk));
			} else
				free_block(&blk);
			pend_clear(&pend);
			continue;
		}
		/* the name of a leading NAME=value assignment */
		j = 0;
		while (isalnum((unsigned char)line[j]) || line[j] == '_')
			j++;
		if (j && line[j] == '=')
			ret = sh_assign(line);
		else
			ret = sh_err("command", line);
	}
	free(body->s);
	free(lb->s);
	pend_clear(&pend);
	if (in_body && ret >= 0)
		ret = sh_err("body", "unterminated printf");
	return ret;
}

static void free_blocks(p2vi_block_t *blks, int nblks)
{
	for (int i = 0; i < nblks; i++)
		free_block(&blks[i]);
	free(blks);
}

/* -e: every block in order, each in its own editor lifetime, stopping at
 * the first failure the way "sh -e" does. */
static int exec_p2vi_script(FILE *in)
{
	p2vi_block_t *blks = NULL;
	int nblks = 0, st = 0, ret = parse_p2vi_script(in, &blks, &nblks);
	for (int i = 0; ret >= 0 && i < nblks; i++) {
		if ((st = run_body(&blks[i])) < 0)
			ret = -1;
		if (st)
			break;
	}
	free_blocks(blks, nblks);
	return ret < 0 ? 1 : st;
}

/*
 * Replay: the same blocks, but as one editor session. Deriving a compat patch
 * means seeing the tree an origin script leaves behind, so the buffers persist
 * across blocks (a later block naming an already edited file switches to that
 * buffer, no disk round-trip), nothing is ever written, and the last block
 * hands the session to the user. Everything that is not a buffer is still reset
 * per block, as under -e: no register cache, "??" tag or separator carries.
 */

#define BODY_DELIM(c) ((c) == sep || (c) == '\n')

/* Drop the body's trailing writes ("b<N> SEP w" per file, then "2q"), parsed
 * from the end: "vis 2", which stays, also occurs inside the quit and interrupt
 * chains and so anchors nothing. */
static int strip_body_tail(char *body, int sep)
{
	int n = strlen(body), s;
	while (n && (body[n - 1] == '\n' || body[n - 1] == sep))
		n--;
	for (s = n; s && !BODY_DELIM(body[s - 1]); s--);
	if (n - s != 2 || strncmp(body + s, "2q", 2))
		return sh_err("body", "no trailing quit");
	for (n = s ? s - 1 : 0; n > 0; ) {
		for (s = n; s && !BODY_DELIM(body[s - 1]); s--);
		if (n - s != 1 || body[s] != 'w')
			break;
		n = s ? s - 1 : 0;
		for (s = n; s && !BODY_DELIM(body[s - 1]); s--);
		if (n - s < 2 || body[s] != 'b')
			return sh_err("body", "write without a buffer");
		n = s ? s - 1 : 0;
	}
	body[n] = '\0';
	return 0;
}

/* b<N> indexes the block's own file list, but a replay session's indices are
 * session-global, so the tokens are rewritten with the session's numbers. Only
 * a whole command counts as one, leaving the literal ":b0:" inside INTR (whose
 * commands are colon-separated) alone. */
static char *remap_bufnums(const char *body, int sep, int *bmap, int nmap)
{
	const char *s = body, *e, *d;
	sbuf_smake(out, SB_INIT)
	while (*s) {
		for (e = s; *e && !BODY_DELIM(*e); e++);
		for (d = s + 1; d < e && isdigit((unsigned char)*d); d++);
		if (*s == 'b' && d > s + 1 && d == e) {
			int n = atoi(s + 1);
			if (n >= nmap) {
				free(out->s);
				sh_err("body", "buffer out of range");
				return NULL;
			}
			sb_printf(out, "b%d", bmap[n]);
		} else
			sbuf_mem(out, s, e - s)
		if (*e)
			sbuf_chr(out, *e++)
		s = e;
	}
	sbufn_ret(out, out->s)
}

/* The session's buffer index for a path, opened if this is the first block to
 * name it; mirrors bufs_open()'s append order. */
static int sess_buf(char ***paths, int *npaths, const char *path)
{
	for (int i = 0; i < *npaths; i++)
		if (!strcmp((*paths)[i], path))
			return i;
	*paths = erealloc(*paths, (*npaths + 1) * sizeof(char *));
	(*paths)[*npaths] = uc_dup(path);
	return (*npaths)++;
}

/* Post-origin baseline: each named buffer's text at the moment the origin
 * (and, for -C, the target) has been replayed but before the user edits it.
 * The compat diff is measured from here to the buffer's final state. */
typedef struct { char *path, *text; } snap_t;
typedef struct { snap_t *v; int n, cap; } snaps_t;
static snaps_t compat_base;

/* Replace sn with one entry per named buffer in the live session. */
static void snap_bufs(snaps_t *sn)
{
	for (int i = 0; i < sn->n; i++) {
		free(sn->v[i].path);
		free(sn->v[i].text);
	}
	sn->n = 0;
	for (int i = 0; i < xbufcur; i++) {
		if (!bufs[i].path || !bufs[i].path[0])
			continue;
		ARR_PUSH(sn->v, sn->n, sn->cap)
		sn->v[sn->n].path = uc_dup(bufs[i].path);
		sn->v[sn->n++].text = lbuf_text(bufs[i].lb);
	}
}

/* The snapshotted text of path, NULL if it was not snapshotted. */
static char *snap_find(snaps_t *sn, const char *path)
{
	for (int i = 0; i < sn->n; i++)
		if (!strcmp(sn->v[i].path, path))
			return sn->v[i].text;
	return NULL;
}

/* A baseline entry holding path's on-disk text, unless it has one. A path no
 * baseline block named is untouched, so disk is its post-replay state; without
 * the entry compat_derive() would take it for "opened after the baseline" and
 * drop the edits made to it. */
static void snap_seed(snaps_t *sn, const char *path)
{
	char **v;
	int n, is_new;
	if (snap_find(sn, path))
		return;
	v = read_lines(path, &n, &is_new);
	sbuf_smake(sb, SB_INIT)
	for (int i = 0; i < n; i++) {
		sbuf_str(sb, v[i])
		sbuf_chr(sb, '\n')
	}
	sbuf_nul(sb)
	free_lines(v, n);
	ARR_PUSH(sn->v, sn->n, sn->cap)
	sn->v[sn->n].path = uc_dup(path);
	sn->v[sn->n++].text = sb->s;
}

/* -C second positional, unified-diff form, applied to the live buffers
 * of the handover session (its script form replays as another block). */
static int compat_apply_diff(const char *path);
static int compat_pre_script;	/* the second positional is a generated script */

/*
 * FAILURE PLACEMENT
 *
 * A QF2=1 run reports every failing site and keeps going, so it ends with the
 * hunks that missed simply not applied - the state is in the buffers, but what
 * was supposed to happen is only in the terminal scrollback. Placement puts it
 * back, using the two things the run leaves behind in registers: REG_FLOG, a
 * line per failure, and the command stream itself (P2VI_REG), which the block
 * ran and which still holds every edit verbatim.
 *
 * The two are joined by the mark. A phase-2 FAIL line reads
 * "<path>:<line>:m<id>", and in the stream a mark address only ever occurs at
 * the head of a whole separator-delimited command, so "<sep>'<id>" names the
 * failed edit and nothing else. What is lost is where it should go: its anchor
 * did not resolve, so the only placement left is the line number the FAIL line
 * carries - the site's line in the original file, which is approximate once the
 * hunks above it have shifted things. So the edit is re-aimed at that line and
 * run; if it fails there too, it goes into the buffer verbatim between marker
 * lines, which loses nothing and is a local edit to fix up. Either way the
 * session is handed over parked on the first such spot.
 */

/* One line of REG_FLOG, cut up where it lies: "FAIL <path>:<line>:m<id>", read
 * off the end so a path holding a colon still resolves. Returns the mark, or -1
 * for a line with none - a phase-1 report, which names an anchor that did not
 * resolve and so no edit to recover; the phase-2 site it was to steer is logged
 * right after it and is the actionable half of the same failure. */
static int fail_parse(char *s, char **path, int *line)
{
	char *c;
	int mark;
	if (strncmp(s, "FAIL ", 5) || !(c = strrchr(s, ':')) || c[1] != 'm'
			|| !isdigit((unsigned char)c[2]))
		return -1;
	mark = atoi(c + 2);
	*c = '\0';
	if (!(c = strrchr(s, ':')) || !isdigit((unsigned char)c[1]))
		return -1;
	*c = '\0';
	*line = atoi(c + 1);
	*path = s + 5;
	return mark;
}

/* The whole command addressing mark <mark>, at whatever depth it sits: a
 * command always starts right after a separator byte, so a quote there is an
 * address and never payload (payload lines are newline-delimited). It ends at
 * the next separator, minus the escapes a nested site carries before it. */
static char *stream_site(const char *body, int mark)
{
	const char *p = body, *e;
	while ((p = strchr(p, xsep))) {
		if (*++p != '\'' || atoi(p + 1) != mark)
			continue;
		for (e = p + 1; isdigit((unsigned char)*e); e++);
		if (e == p + 1 || !(e = strchr(e, xsep)))
			continue;
		while (e[-1] == xesc)
			e--;
		return dup_n(p, e - p);
	}
	return NULL;
}

static int buf_by_path(const char *path)
{
	for (int i = 0; i < xbufcur; i++)
		if (bufs[i].path && !strcmp(bufs[i].path, path))
			return i;
	return -1;
}

/* Put one failure back, in the buffer it belongs to. Returns the row the
 * session should park on, or -1 if that file is not even open. */
static int fail_place(const char *body, const char *path, int line, int mark,
		      int *bi, int *shift)
{
	char *site = stream_site(body, mark);
	const char *addr, *verb;
	int row, len, ok = 0;
	sbuf_smake(sb, SB_INIT)
	if ((*bi = buf_by_path(path)) < 0) {
		free(site);
		free(sb->s);
		return -1;
	}
	bufs_switch(*bi);
	/* every block placed above this one pushed the rest of the file down,
	 * and the failures come in patch order, so the shift simply adds up */
	line += shift[*bi];
	len = lbuf_len(xb);
	row = MAX(0, MIN(len, line) - 1);
	xrow = row;
	xoff = 0;
	if (site) {
		for (addr = site + 1; isdigit((unsigned char)*addr); addr++);
		for (verb = addr; *verb && !isalpha((unsigned char)*verb); verb++);
		/* The mark is what did not resolve, so the same command aimed
		 * at the reported line is the best guess left - taken only
		 * where it cannot lose anything: an insert adds, a substitute
		 * has to match first. A "c" or "d" one line off would quietly
		 * eat a line the patch never mentioned. */
		if (*verb == 'i' || *verb == 's') {
			sb_printf(sb, "%d%s", line, addr);
			sbuf_nul(sb)
			ok = ex_exec(sb->s) == NULL;
			sbuf_cut(sb, 0)
		}
	}
	if (!ok) {
		sb_printf(sb, ">>> p2v FAIL %s:%d:m%d\n", path, line, mark);
		if (site)
			sb_str(sb, site);
		if (sb->s[sb->s_n - 1] != '\n')
			sb_chr(sb, '\n');
		sb_str(sb, "<<< p2v END\n");
		sbuf_nul(sb)
		lbuf_edit(xb, sb->s, row, row, -1, -1);
	}
	shift[*bi] += lbuf_len(xb) - len;
	free(site);
	free(sb->s);
	return row;
}

/* Every failure the block logged, oldest first. Returns the buffer to park the
 * handover on and sets *prow to the row in it, or -1 when the run logged
 * nothing - which is every run that did not fail, so the common path is one
 * register lookup.
 *
 * bodyreg holds the body the logged marks belong to: the driver's own, or the
 * staged section a -E selector run singles out. skip is how many bytes the log
 * already held before that body ran, so a run that reports one section's
 * misses does not place the whole call's. */
static int fail_report(int sepb, int *prow, int bodyreg, int skip)
{
	sbuf *log = ex_regget(REG_FLOG), *body = ex_regget(bodyreg);
	char *s, *nl, *txt, *path;
	int *shift, bi, row, line, mark, first = -1, n = 0;
	if (!log || log->s_n <= skip || !body || !xbufcur)
		return -1;
	shift = ecalloc(xbufcur, sizeof(int));
	/* cut up a copy: the register is the run's own record, which the
	 * handover may well want to read */
	txt = uc_dup(log->s + skip);
	/* the stream is read and re-run under the body's own specials: xesc is
	 * still what its "|sc!" prologue set (the stripped tail never put it
	 * back) and the separator is restated from the block */
	preserve(int, xsep, xsep = sepb;)
	/* every entry is newline-terminated, that being the point of logging
	 * into an upper-case register */
	for (s = txt; (nl = strchr(s, '\n')); s = nl) {
		*nl++ = '\0';
		if ((mark = fail_parse(s, &path, &line)) < 0)
			continue;
		row = fail_place(body->s, path, line, mark, &bi, shift);
		if (row >= 0 && first < 0) {
			first = bi;
			*prow = row;
		}
		n += row >= 0;
	}
	restore(xsep)
	free(txt);
	free(shift);
	if (n)
		fprintf(stderr, "replay: %d failed hunk%s put back at the "
			"reported line; look for \">>> p2v FAIL\"\n",
			n, n > 1 ? "s" : "");
	return first;
}

static int cmd_is(const char *body, int b, int e, const char *s)
{
	int n = strlen(s);
	return e - b == n && !strncmp(body + b, s, n);
}

/* -E reg: lift one section's call out of the driver body, so the caller can
 * run the rest of the body, take the compat baseline, and only then run this
 * one block.
 *
 * emit_driver_call writes a compat section's call as three commands - "2sc %",
 * "? %@<reg>" and "2sc!" - and the register is the block's own, so the middle
 * one names the span unambiguously; the shape around it is checked rather than
 * trusted. Only the call moves: the gate that armed <reg> stays at the head of
 * the body and is a register write, not an edit. What the lift costs is only
 * the order - every other block still runs where it was stored, above the
 * baseline, so its edits cancel out of the derived diff, and the commands the
 * emitter puts before this call (buffer rewinds, quit policy) are state and not
 * text. Returns the register the section body was armed in (fail_report reads
 * the marks out of it), or -1. */
static int body_cut_dispatch(char *body, int sep, int secreg, char **span)
{
	struct { int b, e; } *c = NULL;
	int n = strlen(body), nc = 0, cap = 0, i, t = -1, p = 0, b, e;
	char pat[32];
	for (;;) {
		b = p;
		while (p < n && !BODY_DELIM(body[p]))
			p++;
		ARR_PUSH(c, nc, cap)
		c[nc].b = b;
		c[nc++].e = p;
		if (p >= n)
			break;
		p++;
	}
	snprintf(pat, sizeof(pat), "? %%@%d", secreg);
	for (i = 1; i + 1 < nc; i++)
		if (cmd_is(body, c[i].b, c[i].e, pat)) {
			t = i;
			break;
		}
	if (t < 0) {
		free(c);
		fprintf(stderr, "replay: no section call on register %d\n",
			secreg);
		return -1;
	}
	if (!cmd_is(body, c[t - 1].b, c[t - 1].e, "2sc %")
	    || !cmd_is(body, c[t + 1].b, c[t + 1].e, "2sc!")) {
		free(c);
		fprintf(stderr, "replay: register %d is not a section call\n",
			secreg);
		return -1;
	}
	b = c[t - 1].b;
	e = c[t + 1].e;
	if (e < n)		/* the delimiter goes with the span */
		e++;
	/* The span is lifted past the body's write tail, whose "vis 2" has
	 * left the editor a session by then; the sections are ex scripts and
	 * every one of them was emitted under the prologue's "vis 3". */
	*span = emalloc(e - b + 7);
	i = sprintf(*span, "vis 3%c", sep);
	memcpy(*span + i, body + b, e - b);
	(*span)[i + e - b] = '\0';
	memmove(body + b, body + e, n - e + 1);
	free(c);
	return secreg;
}

/* Every block in one session, leaving its buffers alive for the caller to read
 * back. With handover the last block leaves the editor to the user instead of
 * returning at the end of its body. snap_blk names the block the compat
 * baseline is taken after (-1 = the last): the blocks past it are a pre-applied
 * resolution, which belongs to the derived patch and so must land above the
 * baseline. split_reg is -E's block selector: that block's dispatch is lifted
 * out of snap_blk's driver body and run after the baseline instead of in
 * place, so the baseline is the tree as it stands before the block runs. */
static int replay_blocks(p2vi_block_t *blks, int nblks, int handover,
			 int snap_blk, int split_reg)
{
	char **paths = NULL, *body, *ln;
	int npaths = 0, *bmap = NULL, nmap = 0, i, k, st = 0, sep, bad = 0;
	int fbuf = -1, frow = 0, failreg = P2VI_REG, logskip = 0;
	if (snap_blk < 0 || snap_blk >= nblks)
		snap_blk = nblks - 1;
	/* sized for the union of every block's files: an eviction would
	 * silently drop an edited buffer from the derived patch */
	xbufsalloc = MAX(64, xbufsalloc);
	for (i = 0; i < nblks && st == 0; i++) {
		int last = handover && i == nblks - 1;
		if ((sep = blks[i].sep) < 0) {
			st = -1;
			break;
		}
		/* In a handover session every block claims the tty, not just the
		 * last: term_init() always draws, and a non-final block left on
		 * fd 1 would leak its status line into the script on stdout. */
		if (ed_init(handover) < 0) {
			st = -1;
			break;
		}
		/* the tail's flags govern every block's loads, not just the
		 * first: the body run clears bit 4 below (its own chatter is
		 * not the session's to silence) and it must be back before
		 * the next block loads its files and scaffolds */
		xvis = (hand_vis >= 0 ? hand_vis : xvis) | 2;
		/* the staged section bodies load as scaffolding buffers above
		 * the real files and the driver references both by index, so
		 * the map must cover the whole span */
		int nb = blks[i].npaths + blks[i].nsects;
		if (nb > nmap) {
			nmap = nb;
			bmap = erealloc(bmap, nmap * sizeof(int));
		}
		for (k = 0; k < blks[i].npaths; k++) {
			bmap[k] = sess_buf(&paths, &npaths, blks[i].paths[k]);
			xmpt = 0;
			ec_edit("", "e", blks[i].paths[k]);
		}
		for (k = 0; k < blks[i].nsects; k++) {
			char sname[32];
			snprintf(sname, sizeof(sname), "*p2vi-sec-%d*", k);
			bmap[blks[i].npaths + k] = xbufcur;	/* appended next */
			/* remapped like the driver body below: earlier blocks may
			 * have opened files that shift the real-file numbers */
			char *sec_body = uc_dup(blks[i].sects[k]);
			char *remapped;
			if (!(remapped = remap_bufnums(sec_body, sep, bmap,
							 blks[i].npaths + k + 1))) {
				free(sec_body);
				ed_done();
				st = -1;
				break;
			}
			free(sec_body);
			xmpt = 0;
			ed_loadbuf(sname, remapped);
			free(remapped);
		}
		if (st < 0)
			break;
		xmpt = 0;
		xvis &= ~4;
		body = uc_dup(blks[i].body);
		if (strip_body_tail(body, sep) < 0
		    || !(ln = remap_bufnums(body, sep, bmap, nb))) {
			free(body);
			ed_done();
			st = -1;
			break;
		}
		free(body);
		body = ln;
		ex_regput(P2VI_REG, body, 0);
		if (split_reg >= 0 && i == snap_blk) {
			char *span = NULL;
			int breg = body_cut_dispatch(body, sep, split_reg,
						     &span);
			if (breg < 0) {
				free(body);
				ed_done();
				st = -1;
				break;
			}
			ex_exec(body);
			if (!xquit) {
				sbuf *lg;
				snap_bufs(&compat_base);
				lg = ex_regget(REG_FLOG);
				logskip = lg ? lg->s_n : 0;
				failreg = breg;
				ex_exec(span);
			}
			free(span);
		} else
			ex_exec(body);
		free(body);
		/* Drop the section scaffolding: its %@ calls have run and applied
		 * their edits, so the handover and the read-back must see only the
		 * real files. The section buffers are the topmost slots; switch to
		 * a real one first so ex_buf/ex_pbuf never dangle. */
		if (blks[i].nsects) {
			bufs_switch(bmap[0]);
			ex_pbuf = ex_tpbuf = ex_buf;
			for (k = 0; k < blks[i].nsects; k++)
				bufs_free(--xbufcur);
		}
		if (compat_capturing && !xquit && i == snap_blk) {
			snap_bufs(&compat_base);
			for (k = i + 1; k < nblks; k++)
				for (int p = 0; p < blks[k].npaths; p++)
					snap_seed(&compat_base, blks[k].paths[p]);
			if (compat_pre && !compat_pre_script &&
					compat_apply_diff(compat_pre) < 0) {
				ed_done();
				st = -1;
				break;
			}
		}
		/* a body that quit did so on failure (its own quit tail is
		 * gone), so the user is handed nothing and the status
		 * stands */
		if (last && !xquit) {
			/* Present the baseline as a clean, saved file: the "w" that
			 * would have marked it saved went with the stripped tail, so
			 * without this every buffer shows as modified and undo still
			 * reaches back into the replay's own edits. */
			for (k = 0; k < xbufcur; k++)
				lbuf_saved(bufs[k].lb, 1);
			/* a QF2=1 body reaches here having reported its
			 * failures instead of quitting at the first: put what
			 * they were meant to do back into the buffers, after
			 * the save above so the user can undo any of it. Not
			 * while deriving a compat patch - there a miss is the
			 * input to the derivation, not a hunk to fix, and the
			 * blocks would land in the derived diff. */
			if (!compat_capturing)
				fbuf = fail_report(sep, &frow, failreg,
						   logskip);
			ed_serve(fbuf, frow);
		}
		if (!xquit)	/* no counted quit: the block simply ended */
			xquit = -1;
		st = ed_done();
		bad = i + 1;
		if (i + 1 < nblks || !handover) {
			/* the next block starts as a fresh editor over the same
			 * buffers: saved, so :e and :q see no modification and
			 * undo cannot cross the boundary, and with no state */
			if (xbufcur)
				bufs_switch(0);
			for (k = 0; k < xbufcur; k++) {
				struct lbuf *lb = bufs[k].lb;
				lbuf_saved(lb, 1);
				/* exbuf_save() persists the cursor and the marks
				 * live on the lbuf, so the next block would :e
				 * each file with the previous one's position and
				 * marks - and a non-fatal phase-1 miss would fall
				 * through to a phase-2 edit steered by them.
				 * Rewind and drop the marks, as a freshly opened
				 * file has none. */
				bufs[k].row = bufs[k].off = bufs[k].top = 0;
				lb->mark_n = 0;
				lb->mark_sb[0] = lb->mark_se[0] = -1;
			}
			ed_free_session();
		}
	}
	for (i = 0; i < npaths; i++)
		free(paths[i]);
	free(paths);
	free(bmap);
	if (st > 0)
		fprintf(stderr, "replay: block %d failed with status %d\n",
			bad, st);
	return st;
}

/* Append one script's blocks to *blks. Header assignments are per script and
 * each block carries its own separator, so two scripts' headers never mix.
 *
 * tol replays the script with QF2=1, whatever the environment says. A -C
 * derivation replays a target that is expected to collide with the origin -
 * that collision is the whole input to the derivation - and a target quitting
 * at its first missed hunk would leave a tree the fix was never written
 * against. The shipped script does the same thing on its own: a block's host
 * override empties the quit chain, so a target that already carries one keeps
 * going where the same target, stripped back to its base (rebuild), would die.
 * The misses are still reported. */
static int parse_script(const char *path, p2vi_block_t **blks, int *nblks,
			int tol)
{
	FILE *f = fopen(path, "r");
	int st;
	if (!f) {
		perror(path);
		return -1;
	}
	sh_reset();
	if (tol)
		sh_set("QF2", "1");
	exec_script = path;
	st = parse_p2vi_script(f, blks, nblks);
	fclose(f);
	return st;
}

/* Replay the scripts over the tree as it is on disk, in one session the caller
 * reads back: -C replays the origin and then the target, so the user is handed
 * the state both applied to. The scripts keep their own phase policy - the
 * script itself is the single source of truth. snap_sc is the script index of
 * the baseline snapshot, translated into the block index replay_blocks() wants
 * (a script contributes one block per $VI call). split_reg is passed straight
 * through to replay_blocks. */
static int replay_scripts(const char **paths, int nscripts, int handover,
			  int snap_sc, int split_reg)
{
	p2vi_block_t *blks = NULL;
	int nblks = 0, st = 0, i, snap_blk = -1;
	for (i = 0; i < nscripts && st >= 0; i++) {
		/* the applied set is the origins already replayed: the -e path's
		 * counterpart of the $P2VI_PATCH the shell chain would carry */
		cur_applied_set(paths, i);
		/* the origins are replayed as they ship; only the target is
		 * held to the collision it is being measured through */
		st = parse_script(paths[i], &blks, &nblks,
				  (compat_mode || split_reg >= 0)
				  && i == snap_sc);
		if (i == snap_sc)
			snap_blk = nblks - 1;
	}
	cur_applied_free();
	if (st >= 0)
		st = replay_blocks(blks, nblks, handover, snap_blk, split_reg);
	free_blocks(blks, nblks);
	return st;
}

/*
 * -I: edit in the built-in nextvi and convert what changed into a script.
 * Nothing is written back: every buffer the editor leaves behind is diffed
 * against the file as it was on disk, and that diff feeds the same pipeline a
 * diff read from stdin would - so a session that visits several files with :e
 * yields one script covering all of them. Hence the built-in differ below.
 *
 * -E is the same emit stage over a different session: instead of opening bare
 * files, it replays a generated script and hands over the tree that replay
 * leaves behind. The diff base is still the file on disk, so the new script
 * carries the old one's effect plus the user's changes - it replaces the input
 * script rather than extending it (extending is -C). What it replaces is the
 * base patch only: the stored compat blocks are carried over untouched, since
 * a base edit is the one thing that must not cost the blocks that stack on it.
 */
static int edit_mode;		/* -I: edit, then emit the diff as a script */
static int amend_mode;		/* -E: replay a script, edit, re-emit it */
static int amend_inplace;	/* -oE: -o's file is -E's own script */

#define DIFF_CTX 3		/* context lines around a hunk */

typedef struct {
	char t;		/* ' ' keep, '-' delete, '+' insert */
	char *s;	/* the line, owned by the old/new arrays */
} dop_t;

typedef struct {
	dop_t *v;
	int n, cap;
} dops_t;

static void dop_add(dops_t *d, char t, char *s)
{
	ARR_PUSH(d->v, d->n, d->cap)
	d->v[d->n].t = t;
	d->v[d->n++].s = s;
}

/* Line census over one span, for patience anchoring: how often a line occurs on
 * each side, and where last. Open addressing, strcmp on collision. */
typedef struct {
	const char *s;
	int co, cn;		/* occurrences in old[] / new[] */
	int in;			/* the last index seen in new[] */
} lmap_t;

static lmap_t *lmap_slot(lmap_t *t, unsigned mask, const char *s)
{
	unsigned i = hash_str(s, 5381) & mask;
	while (t[i].s && strcmp(t[i].s, s))
		i = (i + 1) & mask;
	return &t[i];
}

/* Patience anchors for a span the O(NP) search below could not afford: lines
 * occurring exactly once on each side pair up unambiguously, and the longest
 * increasing subsequence of those pairs is a set of matches no sane diff would
 * cross. Splitting there leaves sub-spans the search can afford, so a
 * 2000-line function whose body moved by two lines diffs as the handful of
 * lines that really changed, not as 4000 lines of delete-all/insert-all. */
static int diff_anchors(char **old, int os, int oe, char **new, int ns, int ne,
			int **aop, int **anp)
{
	int n = oe - os, m = ne - ns, nc = 0, len = 0, i, k;
	int *co, *cn, *pile, *prev, *ao, *an;
	unsigned cap = 8, mask;
	lmap_t *t;
	while (cap < (unsigned)(n + m) * 2)
		cap <<= 1;
	mask = cap - 1;
	t = ecalloc(cap, sizeof(*t));
	for (k = os; k < oe; k++) {
		lmap_t *e = lmap_slot(t, mask, old[k]);
		e->s = old[k];
		e->co++;
	}
	for (k = ns; k < ne; k++) {
		lmap_t *e = lmap_slot(t, mask, new[k]);
		e->s = new[k];
		e->cn++;
		e->in = k;
	}
	/* candidates in old order, so the LIS below only has to sort by new */
	co = emalloc(n * sizeof(int));
	cn = emalloc(n * sizeof(int));
	for (k = os; k < oe; k++) {
		lmap_t *e = lmap_slot(t, mask, old[k]);
		if (e->co == 1 && e->cn == 1) {
			co[nc] = k;
			cn[nc++] = e->in;
		}
	}
	free(t);
	if (!nc) {
		free(co);
		free(cn);
		return 0;
	}
	/* longest strictly increasing subsequence of cn[], patience piles:
	 * pile[l] = candidate ending the smallest length-l run so far */
	pile = emalloc((nc + 1) * sizeof(int));
	prev = emalloc(nc * sizeof(int));
	for (k = 0; k < nc; k++) {
		int lo = 1, hi = len, pos;
		while (lo <= hi) {
			int mid = (lo + hi) / 2;
			if (cn[pile[mid]] < cn[k])
				lo = mid + 1;
			else
				hi = mid - 1;
		}
		pos = lo;
		prev[k] = pos > 1 ? pile[pos - 1] : -1;
		pile[pos] = k;
		if (pos > len)
			len = pos;
	}
	ao = emalloc(len * sizeof(int));
	an = emalloc(len * sizeof(int));
	for (k = len - 1, i = pile[len]; k >= 0; k--, i = prev[i]) {
		ao[k] = co[i];
		an[k] = cn[i];
	}
	free(co);
	free(cn);
	free(pile);
	free(prev);
	*aop = ao;
	*anp = an;
	return len;
}

/* Ops turning old[os..oe) into new[ns..ne): common head and tail lines first,
 * so the search only sees what is left, then either the O(NP) search or -
 * where its route recording would outgrow its budget - a patience split into
 * sub-spans around unique common lines, each diffed by the same routine. Only
 * a span both too large and anchorless degrades to delete-all/insert-all. */
/*
 * The minimal edit script by the O(NP) algorithm of Wu, Manber, Myers and
 * Miller, over lines rather than characters.
 *
 * Myers' O(ND) search iterates on D, the whole edit distance, growing the band
 * of diagonals k in [-d,d] around 0. With M <= N, though, N-M insertions are
 * forced by the length difference alone, so D = 2P + delta with P the number of
 * deletions: iterating on P and growing the band around the delta diagonal
 * instead never spends a round rediscovering an insertion the lengths already
 * imply. Our patches are lopsided that way - linewrap_v2 is +513/-88, so P=88
 * where D=601 - and the LCS table this replaces was quadratic in both time and
 * memory, which is what its cell cap and the anchor split existed to contain.
 *
 * The furthest-reaching point on each diagonal is fp[]; what the frontier alone
 * cannot give back is the script, so each step also records the snake it ended
 * at and the point it grew from (pts[], route[]), and the chain from the delta
 * diagonal is walked forward into ops at the end.
 */
/* Recorded snakes worth their ~12 MB. A round records one per diagonal it
 * touches, so a search costs about P*(P+delta) of them: the shipped patches sit
 * in the tens of thousands (linewrap_v2 is P=88, delta=425, ~45k), and a span
 * that would outgrow this is one the anchor split below breaks up anyway. */
#define ONP_MAX_PTS 1000000

typedef struct {
	int x, y;	/* the snake's end */
	int prev;	/* the pts[] entry it grew from, -1 at the root */
} onp_pt;

typedef struct {
	char **a, **b;		/* a is the shorter side, b the longer */
	int m, n, offset;
	int *fp, *route;	/* per diagonal: furthest y, and its pts[] entry */
	onp_pt *pts;
	int npts, cap;
} onp_t;

/* One diagonal's step: come from whichever neighbour reaches further, run the
 * snake of equal lines out, and record where it ended. */
static int onp_snake(onp_t *o, int k)
{
	int kk = k + o->offset;
	int yd = o->fp[kk - 1] + 1, yr = o->fp[kk + 1];
	int down = yd > yr;
	int y = down ? yd : yr;
	int x = y - k;
	int prev = down ? o->route[kk - 1] : o->route[kk + 1];
	while (x < o->m && y < o->n && !strcmp(o->a[x], o->b[y])) {
		x++;
		y++;
	}
	ARR_PUSH(o->pts, o->npts, o->cap)
	o->pts[o->npts].x = x;
	o->pts[o->npts].y = y;
	o->pts[o->npts].prev = prev;
	o->route[kk] = o->npts++;
	return y;
}

/* The script itself: the chain of snakes, oldest first, walked forward. Between
 * two of them the path is one step off the diagonal - right for a line of a,
 * down for a line of b - and then equal lines until the next snake's end. With
 * the sides swapped a is the new file, so which step is a delete swaps with it. */
static void onp_script(onp_t *o, dops_t *d, int swap)
{
	char ins = swap ? '-' : '+', del = swap ? '+' : '-';
	int *chain = emalloc((o->npts + 1) * sizeof(int));
	int nc = 0, px = 0, py = 0, i, r;
	for (r = o->route[o->n - o->m + o->offset]; r >= 0; r = o->pts[r].prev)
		chain[nc++] = r;
	for (i = nc - 1; i >= 0; i--) {
		onp_pt *e = &o->pts[chain[i]];
		while (px < e->x || py < e->y) {
			if (e->y - e->x > py - px)
				dop_add(d, ins, o->b[py++]);
			else if (e->y - e->x < py - px)
				dop_add(d, del, o->a[px++]);
			else {
				dop_add(d, ' ', o->a[px++]);
				py++;
			}
		}
	}
	free(chain);
}

/* 0 and the ops of old[os,oe) -> new[ns,ne), or -1 with nothing added when the
 * route recording would outgrow its budget - the one case left for the anchor
 * split below. Neither side may be empty; the caller has trimmed both ends. */
static int diff_onp(dops_t *d, char **old, int os, int oe,
		    char **new, int ns, int ne)
{
	onp_t o;
	int m = oe - os, n = ne - ns, swap = m > n, delta, size, p = -1, k, st;
	memset(&o, 0, sizeof(o));
	o.a = swap ? new + ns : old + os;
	o.b = swap ? old + os : new + ns;
	o.m = swap ? n : m;
	o.n = swap ? m : n;
	delta = o.n - o.m;
	o.offset = o.m + 1;
	size = o.m + o.n + 3;
	o.fp = emalloc(size * sizeof(int));
	o.route = emalloc(size * sizeof(int));
	for (k = 0; k < size; k++) {
		o.fp[k] = -1;
		o.route[k] = -1;
	}
	do {
		p++;
		/* the round about to run touches delta + 2p + 1 diagonals and
		 * records one snake on each */
		if (o.npts + delta + 2 * p + 1 > ONP_MAX_PTS)
			break;
		for (k = -p; k <= delta - 1; k++)
			o.fp[k + o.offset] = onp_snake(&o, k);
		for (k = delta + p; k >= delta + 1; k--)
			o.fp[k + o.offset] = onp_snake(&o, k);
		o.fp[delta + o.offset] = onp_snake(&o, delta);
	} while (o.fp[delta + o.offset] != o.n);
	st = o.fp[delta + o.offset] == o.n ? 0 : -1;
	if (!st)
		onp_script(&o, d, swap);
	free(o.fp);
	free(o.route);
	free(o.pts);
	return st;
}

static void diff_region(dops_t *d, char **old, int os, int oe,
			char **new, int ns, int ne)
{
	int n, m, nsuf = 0, na = 0, i, j, k;
	int *ao = NULL, *an = NULL;
	while (os < oe && ns < ne && !strcmp(old[os], new[ns])) {
		dop_add(d, ' ', old[os]);
		os++;
		ns++;
	}
	while (oe > os && ne > ns && !strcmp(old[oe - 1], new[ne - 1])) {
		oe--;
		ne--;
		nsuf++;
	}
	n = oe - os;
	m = ne - ns;
	if (n > 0 && m > 0) {
		if (!diff_onp(d, old, os, oe, new, ns, ne))
			goto tail;
		/* only a span whose search outgrew its budget gets here */
		na = diff_anchors(old, os, oe, new, ns, ne, &ao, &an);
	}
	if (na > 0) {
		int po = os, pn = ns;
		for (k = 0; k < na; k++) {
			diff_region(d, old, po, ao[k], new, pn, an[k]);
			dop_add(d, ' ', old[ao[k]]);
			po = ao[k] + 1;
			pn = an[k] + 1;
		}
		diff_region(d, old, po, oe, new, pn, ne);
		free(ao);
		free(an);
		goto tail;
	}
	/* one side empty, or too big and anchorless: the span goes out whole */
	for (i = os; i < oe; i++)
		dop_add(d, '-', old[i]);
	for (j = ns; j < ne; j++)
		dop_add(d, '+', new[j]);
tail:
	/* the tail trimmed above closes the span, after whatever filled it */
	for (k = 0; k < nsuf; k++)
		dop_add(d, ' ', old[oe + k]);
}

/*
 * Where a run of changed lines sits when it could sit elsewhere.
 *
 * A run bounded by a line equal to the run's own far end can be slid across it:
 * which of two equal lines counts as the changed one is free, and one
 * arbitrary choice does. The choice is not free to the anchor
 * generators, though - it decides what text a hunk carries as context - so a
 * diff derived here has to land where the hand-written one did, or a
 * regenerated script anchors on lines the shipped one never mentioned.
 *
 * git's rule (xdl_change_compact), and the one followed here: slide a run to
 * join a neighbouring run if it can, preferring the run before it, and
 * otherwise slide it as far down as it goes. Joining is what turns "insert a
 * function" into one hunk instead of two split around the brace and blank line
 * that both functions end with.
 *
 * Sliding is per file, as it is in git: a delete run in the old file and an
 * insert run in the new one move independently, so the op list is turned into
 * one changed-line flag per side, slid there, and rebuilt. A slide only ever
 * trades a line for an equal one on the same side, so the unchanged lines of
 * the two files stay the same sequence and still pair up in order.
 */

/* The changed-line flags of an op list: co over the old side, cn over the new. */
static void dops_flags(dops_t *d, char *co, char *cn)
{
	int i, o = 0, n = 0;
	for (i = 0; i < d->n; i++) {
		if (d->v[i].t == ' ') {
			o++;
			n++;
		} else if (d->v[i].t == '-')
			co[o++] = 1;
		else
			cn[n++] = 1;
	}
}

/* How many single-line slides the run [s,e) has in it, dir -1 up or 1 down;
 * *merge is set when what stops the slide is the neighbouring run, i.e. when
 * sliding that far makes the two adjacent. Nothing is written: a slide only
 * moves the flag off one line and onto an equal one, so every step's test is
 * over lines that stay where they are. */
static int chg_slide(char **ln, char *chg, int n, int s, int e, int dir,
		     int *merge)
{
	int k;
	*merge = 0;
	for (k = 1;; k++) {
		int c = dir < 0 ? s - k : e + k - 1;	/* the line crossed */
		int f = dir < 0 ? e - k : s + k - 1;	/* the end it trades with */
		if (c < 0 || c >= n)
			break;
		if (chg[c]) {
			*merge = 1;
			break;
		}
		if (strcmp(ln[c], ln[f]))
			break;
	}
	return k - 1;
}

/* Do the slide chg_slide() counted. */
static void chg_shift(char *chg, int s, int e, int dir, int n)
{
	for (int k = 1; k <= n; k++) {
		if (dir < 0) {
			chg[s - k] = 1;
			chg[e - k] = 0;
		} else {
			chg[e + k - 1] = 1;
			chg[s + k - 1] = 0;
		}
	}
}

/* Slide every run of one side into the position above, left to right. A run
 * that joined its neighbour is left alone afterwards: the scan resumes past
 * the whole merged region, so no part of a run is ever slid on its own. */
static void chg_compact(char **ln, char *chg, int n)
{
	int i = 0;
	while (i < n) {
		int s, e, up, down, upm, downm;
		if (!chg[i]) {
			i++;
			continue;
		}
		s = i;
		while (i < n && chg[i])
			i++;
		e = i;
		up = chg_slide(ln, chg, n, s, e, -1, &upm);
		down = chg_slide(ln, chg, n, s, e, 1, &downm);
		if (upm)
			chg_shift(chg, s, e, -1, up);
		else if (down)
			chg_shift(chg, s, e, 1, down);
		while (i < n && chg[i])
			i++;
	}
}

/* The op list the flags describe, deletes before inserts inside each change
 * region, the unchanged lines pairing off in order. Rebuilt over the list it
 * came from - the lines belong to the caller's arrays either way. */
static void dops_rebuild(dops_t *d, char **old, char *co, int nold,
			 char **new, char *cn, int nnew)
{
	int i = 0, j = 0;
	d->n = 0;
	while (i < nold || j < nnew) {
		if (i < nold && co[i]) {
			while (i < nold && co[i])
				dop_add(d, '-', old[i++]);
		} else if (j < nnew && cn[j]) {
			while (j < nnew && cn[j])
				dop_add(d, '+', new[j++]);
		} else if (i < nold && j < nnew) {
			dop_add(d, ' ', old[i++]);
			j++;
		} else		/* unreachable: both sides keep the same
				 * unchanged lines, so they run out together */
			break;
	}
}

/* An op list as unified diff text for path: header, then one hunk per run of
 * changes with DIFF_CTX context lines around it. Nothing is written when the
 * list holds no change; the op list is the only input, so every way of deriving
 * one serializes through here. */
static void emit_dops(sbuf *out, const char *path, int is_new, dops_t *dp)
{
	dops_t d = *dp;
	int i, j, k, start, end, last, oc, nc, changed = 0;
	int *ono, *nno;
	for (i = 0; i < d.n; i++)
		if (d.v[i].t != ' ')
			changed = 1;
	if (!changed)
		return;
	/* the original and the new line number each op sits at */
	ono = emalloc((d.n + 1) * sizeof(int));
	nno = emalloc((d.n + 1) * sizeof(int));
	for (i = 0, j = 1, k = 1; i < d.n; i++) {
		ono[i] = j;
		nno[i] = k;
		j += d.v[i].t != '+';
		k += d.v[i].t != '-';
	}
	ono[d.n] = j;
	nno[d.n] = k;
	if (is_new)
		sbuf_str(out, "--- /dev/null\n")
	else {
		sbuf_str(out, "--- a/")
		sbuf_str(out, path)
		sbuf_chr(out, '\n')
	}
	sbuf_str(out, "+++ b/")
	sbuf_str(out, path)
	sbuf_chr(out, '\n')
	for (i = 0; i < d.n; ) {
		if (d.v[i].t == ' ') {
			i++;
			continue;
		}
		/* to the last change whose context still touches this one's */
		start = MAX(i - DIFF_CTX, 0);
		last = i;
		for (j = i; j < d.n; ) {
			if (d.v[j].t != ' ') {
				last = j++;
				continue;
			}
			for (k = j; k < d.n && d.v[k].t == ' '; k++)
				;
			if (k >= d.n || k - j > 2 * DIFF_CTX)
				break;
			j = k;
		}
		end = last + 1 + DIFF_CTX;
		if (end > d.n)
			end = d.n;
		oc = nc = 0;
		for (k = start; k < end; k++) {
			oc += d.v[k].t != '+';
			nc += d.v[k].t != '-';
		}
		sb_printf(out, "@@ -%d,%d +%d,%d @@\n",
			  oc ? ono[start] : ono[start] - 1, oc,
			  nc ? nno[start] : nno[start] - 1, nc);
		for (k = start; k < end; k++) {
			sbuf_chr(out, d.v[k].t)
			sbuf_str(out, d.v[k].s)
			sbuf_chr(out, '\n')
		}
		i = end;
	}
	free(ono);
	free(nno);
}

/* The difference between old[] and new[] as a unified diff for path. */
static void emit_unified_diff(sbuf *out, const char *path, int is_new,
			      char **old, int nold, char **new, int nnew)
{
	dops_t d;
	char *co, *cn;
	int pre = 0, suf = 0, i;
	memset(&d, 0, sizeof(d));
	/* the O(NP) search only ever sees what head and tail trimming leaves */
	while (pre < nold && pre < nnew && !strcmp(old[pre], new[pre]))
		pre++;
	while (suf < nold - pre && suf < nnew - pre &&
	       !strcmp(old[nold - 1 - suf], new[nnew - 1 - suf]))
		suf++;
	for (i = 0; i < pre; i++)
		dop_add(&d, ' ', old[i]);
	diff_region(&d, old, pre, nold - suf, new, pre, nnew - suf);
	for (i = nold - suf; i < nold; i++)
		dop_add(&d, ' ', old[i]);
	co = ecalloc(nold + 1, 1);
	cn = ecalloc(nnew + 1, 1);
	dops_flags(&d, co, cn);
	chg_compact(old, co, nold);
	chg_compact(new, cn, nnew);
	dops_rebuild(&d, old, co, nold, new, cn, nnew);
	free(co);
	free(cn);
	emit_dops(out, path, is_new, &d);
	free(d.v);
}

/* Text as a line array, newlines stripped. The text is consumed in place. */
static char **split_lines(char *text, int *n)
{
	char **v = NULL, *p, *nl;
	int cap = 0;
	*n = 0;
	for (p = text; *p; p = nl + 1) {
		if ((nl = strchr(p, '\n')))
			*nl = '\0';
		arr_append(&v, n, &cap, p);
		if (!nl)
			break;
	}
	return v;
}

static void parse_diff_text(const char *text);
static void parse_diff_reset(void);

/* The -C second positional's form, sniffed from its first line as the ordinary
 * input is: 1 = a generated script (replayed as another block), 0 = a unified
 * diff (spliced into the live buffers), -1 = unreadable. */
static int compat_pre_isscript(const char *path)
{
	FILE *f = fopen(path, "r");
	int rc = 0;
	if (!f) {
		perror(path);
		return -1;
	}
	sbuf_smake(lb, SB_INIT)
	if (read_line(f, lb))
		rc = !strncmp(lb->s, "#!/bin/sh", 9);
	free(lb->s);
	fclose(f);
	return rc;
}

/* Where img[] sits in lines[], at or after from, preferring the occurrence
 * nearest hint - the diff's own coordinate, stale by construction on a tree the
 * origin and the target already changed. An empty image is a pure insertion:
 * hint itself, clamped. */
static int find_image(char **lines, int nlines, char **img, int nimg,
		      int from, int hint)
{
	int best = -1, i, j;
	if (!nimg)
		return hint < from ? from : (hint > nlines ? nlines : hint);
	for (i = from; i + nimg <= nlines; i++) {
		for (j = 0; j < nimg && !strcmp(lines[i + j], img[j]); j++)
			;
		if (j < nimg)
			continue;
		if (best < 0 || abs(i - hint) < abs(best - hint))
			best = i;
	}
	return best;
}

/* The live session buffer holding path, opened if no replayed block named it -
 * in which case disk is its baseline text, so it is snapshotted too. */
static struct lbuf *compat_openbuf(char *path)
{
	int i = buf_by_path(path);
	if (i < 0) {
		snap_seed(&compat_base, path);
		xmpt = 0;
		ec_edit("", "e", path);
		i = buf_by_path(path);
	}
	return i < 0 ? NULL : bufs[i].lb;
}

/* One parsed file of the pre-applied diff onto its session buffer: each hunk's
 * pre-image is searched for rather than trusted at its line number, and the
 * buffer rebuilt around the post-image. A hunk whose pre-image is gone is a
 * hard error - dropping it would ship a compat patch nobody wrote. */
static int compat_apply_file(file_patch_t *fp)
{
	struct lbuf *lb;
	char **lines, **out = NULL, **old = NULL, **new = NULL, *text;
	int nlines, nout = 0, ocap = 0, ncap = 0, cap = 0;
	int nold, nnew, cur = 0, i, k, lo, hi, pos, hint, st = 0;
	if (!(lb = compat_openbuf(fp->path))) {
		fprintf(stderr, "%s: cannot open %s\n", compat_pre, fp->path);
		return -1;
	}
	text = lbuf_text(lb);	/* consumed in place by split_lines() */
	lines = split_lines(text, &nlines);
	for (i = 0; i < fp->nops && st == 0;) {
		lo = fp->ops[i].hunk_lo;
		hi = fp->ops[i].hunk_hi;
		nold = nnew = 0;
		while (i < fp->nops && fp->ops[i].hunk_lo == lo &&
				fp->ops[i].hunk_hi == hi) {
			op_t *o = &fp->ops[i++];
			if (o->type != 'a')
				arr_append(&old, &nold, &ocap, o->text);
			if (o->type != 'd')
				arr_append(&new, &nnew, &ncap, o->text);
		}
		/* the hunk's first original line, 0-based; a pure insertion goes
		 * after original line lo, which is that same index */
		hint = nold ? lo - 1 : lo;
		pos = find_image(lines, nlines, old, nold, cur, hint);
		if (pos < 0) {
			fprintf(stderr, "%s: %s: hunk at line %d does not apply\n",
				compat_pre, fp->path, lo);
			st = -1;
			break;
		}
		for (k = cur; k < pos; k++)
			arr_append(&out, &nout, &cap, lines[k]);
		for (k = 0; k < nnew; k++)
			arr_append(&out, &nout, &cap, new[k]);
		cur = pos + nold;
		free_lines(old, nold);
		free_lines(new, nnew);
		old = new = NULL;
		nold = nnew = ocap = ncap = 0;
	}
	if (st == 0) {
		for (k = cur; k < nlines; k++)
			arr_append(&out, &nout, &cap, lines[k]);
		sbuf_smake(sb, SB_INIT)
		for (k = 0; k < nout; k++) {
			sbuf_str(sb, out[k])
			sbuf_chr(sb, '\n')
		}
		sbuf_nul(sb)
		lbuf_edit(lb, sb->s, 0, lbuf_len(lb), 0, 0);
		free(sb->s);
	}
	free_lines(old, nold);
	free_lines(new, nnew);
	free_lines(out, nout);
	free_lines(lines, nlines);
	free(text);
	return st;
}

/* Drop the tail of files[] from `first` on: the entries a scoped parse added
 * once whatever needed them is done with them. */
static void drop_files_from(int first)
{
	for (int i = first; i < nfiles; i++) {
		for (int j = 0; j < files[i].nops; j++)
			free(files[i].ops[j].text);
		free(files[i].ops);
		free(files[i].path);
		free(files[i].orig_path);
	}
	nfiles = first;
}

/* Keep the slots, drop what they patch. A range in the middle of files[] cannot
 * be removed - every later block's range is indexed by position - and a file
 * with no ops builds no groups, which is what every reader downstream tests. */
static void blank_files_range(int first, int count)
{
	for (int i = first; i < first + count && i < nfiles; i++) {
		for (int j = 0; j < files[i].nops; j++)
			free(files[i].ops[j].text);
		free(files[i].ops);
		files[i].ops = NULL;
		files[i].nops = files[i].ops_cap = 0;
	}
}

/* -C second positional, unified-diff form: parsed into its own files[] range and
 * raw sink (so the host === PATCH === stays byte-identical) and spliced into the
 * live buffers. The range is dropped right after - the diff is applied, not
 * shipped, and what the user is left holding is re-diffed from the baseline. */
static int compat_apply_diff(const char *path)
{
	strv_t sink;
	char *text = file_text(path);
	int first = nfiles, st = 0, i;
	memset(&sink, 0, sizeof(sink));
	if (!text) {
		perror(path);
		return -1;
	}
	raw_sink = &sink;
	parse_diff_reset();
	parse_diff_text(text);
	raw_sink = NULL;
	for (i = first; i < nfiles && st == 0; i++)
		st = compat_apply_file(&files[i]);
	drop_files_from(first);
	free_lines(sink.v, sink.n);
	free(text);
	return st;
}

/* The src= label of this derivation: its origins in replay order, one src=
 * field each, so the label of a two-origin block reads "a.sh src=b.sh" and the
 * header carries "src=a.sh src=b.sh". The field is repeated rather than one
 * field with a separator in it because a separator would be a byte no origin
 * path may hold, and the only such byte here is the space the header already
 * parses on: a reader splits on whitespace it could not have kept anyway.
 * Annotation only - the reader keeps the label whole and nothing matches on it
 * - so a single origin still writes exactly its own path. */
static char *compat_origin_label(void)
{
	sbuf_smake(sb, SB_INIT)
	for (int i = 0; i < ncompat_origin; i++) {
		if (i)
			sbuf_str(sb, " src=")
		sbuf_str(sb, compat_origins[i])
	}
	sbufn_ret(sb, sb->s)
}

/* One diff over every buffer the session reshaped, in buffer order: one block,
 * one section, one storage region. A buffer with no baseline was opened after
 * the snapshot (the handover's own command line) and is not part of what is
 * being measured. Returns how many buffers moved. */
static int derive_diff(sbuf *diff)
{
	int nchanged = 0;
	for (int i = 0; i < xbufcur; i++) {
		char **pre, **base, **fin;
		char *basetext = NULL, *fintext, *bdup, *fdup;
		int npre, nbase, nfin, is_new;
		if (!bufs[i].path || !bufs[i].path[0])
			continue;
		basetext = snap_find(&compat_base, bufs[i].path);
		if (!basetext)		/* opened after the baseline: not ours */
			continue;
		fintext = lbuf_text(bufs[i].lb);
		if (!strcmp(basetext, fintext)) {	/* user left it as it was */
			free(fintext);
			continue;
		}
		bdup = uc_dup(basetext);
		fdup = uc_dup(fintext);
		base = split_lines(bdup, &nbase);
		fin = split_lines(fdup, &nfin);
		pre = read_lines(bufs[i].path, &npre, &is_new);
		emit_unified_diff(diff, bufs[i].path, is_new, base, nbase,
				  fin, nfin);
		nchanged++;
		free(fintext); free(bdup); free(fdup);
		free(base); free(fin);
		free_lines(pre, npre);
	}
	return nchanged;
}

/* The one compat block this run produces: replay the origin and the target into
 * one session, hand it to the user, then measure every changed buffer from its
 * post-origin baseline to its final state and concatenate the results into a
 * single unified diff. That diff, over however many files it spans, IS the
 * compatibility patch. The block is stored, not emitted, and its bytes are
 * marked used so the script-global SEP/ESC chosen after this cover them. -1 on
 * any hard error (a nonzero handover status, a session that changed nothing),
 * and main() then writes nothing. */
static int compat_derive(void)
{
	int i, nsc = 0, nchanged = 0;
	int nor = ncompat_origin;
	const char **sc = emalloc((nor + 2) * sizeof(*sc));
	sbuf_smake(diff, SB_INIT)
	for (i = 0; i < nor; i++)
		sc[nsc++] = compat_origins[i];
	sc[nsc++] = input_file;
	/* A pre-applied resolution in script form is one more replay block, run
	 * after the baseline is taken; its diff form is spliced into the buffers
	 * at that same point. Either way the user edits on top of it. */
	if (compat_pre) {
		if ((compat_pre_script = compat_pre_isscript(compat_pre)) < 0) {
			free(sc);
			free(diff->s);
			return -1;
		}
		if (compat_pre_script)
			sc[nsc++] = compat_pre;
	}
	compat_capturing = 1;
	/* Replay the origins in order and then the target into one session so
	 * the new block derives on top of every block the target already
	 * carries; existing compat blocks stack in stored order (post-only, one
	 * group). The baseline is snapshotted after the target. */
	if (replay_scripts(sc, nsc, 1, nor, -1) != 0) {
		ed_free();
		free(sc);
		free(diff->s);
		return -1;
	}
	compat_capturing = 0;
	nchanged = derive_diff(diff);
	ed_free();
	sbuf_nul(diff)
	if (!nchanged) {
		fprintf(stderr, "no compat patch derived\n");
		free(diff->s);
		free(sc);
		return -1;
	}
	ARR_PUSH(compat_blocks, ncompat, compat_cap)
	compat_block_t *cb = &compat_blocks[ncompat++];
	cb->origin = compat_origin_label();
	/* the block's diff parses into a fresh files[] range and its own raw
	 * sink, so the host === PATCH === stays byte-identical */
	raw_sink = &cb->raw;
	parse_diff_reset();
	cb->first = nfiles;
	parse_diff_text(diff->s);
	cb->count = nfiles - cb->first;
	raw_sink = NULL;
	mark_bytes_used(diff->s);
	free(diff->s);
	free(sc);
	return 0;
}

/* -E reg: rebuild one stored compat block, in place.
 *
 * The block's own src= label names the stack it was derived in, so the origins
 * are looked for beside the target and replayed ahead of it: the identity
 * gates then fire exactly as the shell chain would make them, and the tree the
 * user is handed is the one the block is meant to repair. The target replays
 * with QF2=1 forced - a block being amended is a block expected to miss - and
 * with its own dispatch for this block lifted to the end, the baseline taken
 * in between. So the block's own edits, its misses as fail_report puts them
 * back, and whatever the user does on top are together the new diff.
 *
 * Nothing else in the script moves: the host patch, the sibling blocks and
 * this block's label and register are all stored, and re-emitted from storage.
 */
static int amend_derive(void)
{
	compat_block_t *cb;
	char **src = NULL;
	const char **sc = NULL;
	int nsrc = 0, nsc = 0, dlen, i, blk, st = -1;
	sbuf_smake(diff, SB_INIT)
	blk = amend_sel - REG_SEC_BASE;
	if (blk < 0 || blk >= ncompat) {
		fprintf(stderr, "%s: no compat block on register %d\n",
			input_file, amend_sel);
		free(diff->s);
		return -1;
	}
	cb = &compat_blocks[blk];
	nsrc = compat_src_fields(cb, &src);
	sc = emalloc((nsrc + 1) * sizeof(*sc));
	/* the label stores basenames, and a chain is one directory of scripts:
	 * each field grows the target's own directory in place, so src[] stays
	 * the one owner of the strings and sc[] only borrows them */
	dlen = base_name(input_file) - input_file;
	for (i = 0; i < nsrc; i++) {
		char *q = emalloc(dlen + strlen(src[i]) + 1);
		memcpy(q, input_file, dlen);
		strcpy(q + dlen, src[i]);
		free(src[i]);
		src[i] = q;
		sc[nsc++] = q;
		if (access(q, R_OK) < 0) {
			fprintf(stderr, "%s: origin %s of block %d is not "
				"beside the script\n", input_file, q, amend_sel);
			goto out;
		}
	}
	sc[nsc++] = input_file;
	fprintf(stderr, "amend: replaying");
	for (i = 0; i < nsc; i++)
		fprintf(stderr, " %s", sc[i]);
	fprintf(stderr, "\n");
	if (replay_scripts(sc, nsc, 1, nsc - 1, amend_sel) != 0) {
		ed_free();
		goto out;
	}
	if (!derive_diff(diff)) {
		ed_free();
		fprintf(stderr, "no compat patch derived\n");
		goto out;
	}
	ed_free();
	sbuf_nul(diff)
	/* The rebuilt block takes the old one's place: same register, same
	 * label, same position in the run order. Its old files[] range stays
	 * where it is, emptied - the new one parses in at the end of the array,
	 * as a freshly derived block's does. */
	blank_files_range(cb->first, cb->count);
	free_lines(cb->raw.v, cb->raw.n);
	memset(&cb->raw, 0, sizeof(cb->raw));
	raw_sink = &cb->raw;
	parse_diff_reset();
	cb->first = nfiles;
	parse_diff_text(diff->s);
	cb->count = nfiles - cb->first;
	raw_sink = NULL;
	mark_bytes_used(diff->s);
	st = 0;
out:
	free_lines(src, nsrc);
	free(sc);
	free(diff->s);
	return st;
}

/* One buffer left behind by the session, against the file it names. */
static void buf_to_diff(sbuf *out, const char *path, struct lbuf *lb)
{
	char **old, **new, *text;
	int nold, nnew, is_new;
	old = read_lines(path, &nold, &is_new);
	text = lbuf_text(lb);
	new = split_lines(text, &nnew);
	emit_unified_diff(out, path, is_new, old, nold, new, nnew);
	free(text);
	free_lines(old, nold);
	free_lines(new, nnew);
}

/* An editing session over args, its resulting unified diff written to out.
 * Every buffer the session leaves behind is diffed, not just the ones named
 * here: files reached with :e join the same diff, in the order they were
 * opened, and a path that does not exist yet is diffed as a creation.
 *
 * The session is nextvi's own main(), renamed nextvi_main() for this build, so
 * args is a nextvi command line - flags, files and EXINIT behave exactly as in
 * vi(1). Only the framing is patch2vi's: the terminal is claimed first because
 * stdout is the script, and the buffers are read back and freed after, which is
 * the whole point - nextvi_main() returns without touching them and nothing is
 * ever written to disk. Its process-wide bring-up doubles as ed_init()'s, so a
 * later session must not repeat it. */
static int edit_to_diff(char **args, int nargs, sbuf *out)
{
	char **argv;
	int i, st;
	/* every buffer of the session ends up in the diff, so the session
	 * gets room for more of them than a plain editor would keep */
	xbufsalloc = MAX(64, xbufsalloc);
	if (ed_grabtty() < 0)
		return -1;
	/* one slot past argc, NULL: ex_init() steps to the next file before it
	 * tests the count, so it reads argv[argc] the way a real argv is read,
	 * and a real argv is NULL terminated by the C runtime */
	argv = emalloc((nargs + 2) * sizeof(argv[0]));
	argv[0] = "vi";
	for (i = 0; i < nargs; i++)
		argv[i + 1] = args[i];
	argv[nargs + 1] = NULL;
	st = nextvi_main(nargs + 1, argv);
	free(argv);
	ed_once = 1;
	ed_ungrabtty();
	if (st != 0) {
		fprintf(stderr, "editor exited with error %d\n", st);
		ed_free();
		return -1;
	}
	for (i = 0; i < xbufcur; i++)
		if (bufs[i].path && bufs[i].path[0])
			buf_to_diff(out, bufs[i].path, bufs[i].lb);
	ed_free();
	return 0;
}

/* -E: everything after the script name is a nextvi command line, kept for the
 * handed-over session - option letters as in vi(1), then files. */
static int parse_hand_args(char **args, int n)
{
	int i, j, vis = 0;
	for (i = 0; i < n && args[i][0] == '-'; i++) {
		if (args[i][1] == '-' && !args[i][2]) {
			i++;
			break;
		}
		for (j = 1; args[i][j]; j++) {
			if (args[i][j] == 's')
				vis |= 1|2;
			else if (args[i][j] == 'e')
				vis |= 2;
			else if (args[i][j] == 'm')
				vis |= 4;
			else if (args[i][j] == 'a')
				vis |= 8;
			else if (args[i][j] == 'v')
				vis = 0;
			else {
				fprintf(stderr, "Unknown editor option: -%c\n",
					args[i][j]);
				return -1;
			}
		}
		hand_vis = vis;
	}
	hand_files = args + i;
	nhand_files = n - i;
	return 0;
}

/* -o: the script goes to a named file instead of stdout. It is built in a temp
 * file beside it and renamed over it once the whole run succeeded, so a failure
 * anywhere - a refused replay, an unusable separator, a signal - leaves that
 * file untouched and no half-built script is ever visible under its name. That
 * is what makes "-o" naming the very script -E is updating safe: by the time
 * anything is written the input has been replayed, read and closed, whereas a
 * shell redirection onto it would have truncated it before patch2vi started. */
static const char *out_file;
static char *out_tmp;

/* The temporary twin's name is derived deterministically from the target's,
 * so nothing has to keep it from piling up: a run that dies without notice
 * (a signal, the editor's out-of-memory exits under -I) may leave
 * <path>.p2v.tmp behind, and the next -o path run reopens it with "w" and
 * truncates it away. What no run ever leaves is a half-built script under
 * the target's own name - only out_commit() puts one there, atomically. */
static void out_cleanup(void)
{
	if (out_tmp) {
		unlink(out_tmp);
		free(out_tmp);
		out_tmp = NULL;
	}
}

static int out_redirect(const char *path)
{
	struct stat st;
	out_tmp = emalloc(strlen(path) + sizeof(".p2v.tmp"));
	strcpy(out_tmp, path);
	strcat(out_tmp, ".p2v.tmp");
	fflush(stdout);
	if (!freopen(out_tmp, "w", stdout)) {
		perror(out_tmp);
		return -1;
	}
	/* What is emitted is a script meant to be run: a file being replaced
	 * keeps its own mode, a file created here gets the executable bits the
	 * umask allows, so no caller has to chmod what patch2vi wrote. */
	if (!stat(path, &st)) {
		chmod(out_tmp, st.st_mode);
	} else {
		mode_t um = umask(0);
		umask(um);
		chmod(out_tmp, 0777 & ~um);
	}
	return 0;
}

static int out_commit(const char *path)
{
	if (fflush(stdout) || ferror(stdout)) {
		perror(out_tmp);
		out_cleanup();
		return -1;
	}
	if (rename(out_tmp, path) < 0) {
		perror(path);
		out_cleanup();
		return -1;
	}
	free(out_tmp);
	out_tmp = NULL;
	return 0;
}

/* -E: replay one generated script into a single session, hand it to the user,
 * and diff every buffer it leaves behind against disk. The replay is the same
 * one -C drives, so the blocks keep their own phase policy and nothing is
 * written; a block that fails aborts the whole thing, since a partial replay
 * would silently drop the hunks it never reached from the emitted script. */
static int amend_to_diff(const char *path, sbuf *out)
{
	const char *sc[1];
	int i;
	sc[0] = path;
	xbufsalloc = MAX(64, xbufsalloc);
	/* The base is what this replay measures, so no identity gate may fire:
	 * a block that fired would put its edits in the buffers, and they would
	 * be re-derived into the host patch while the block itself stays stored
	 * and gated - applied twice on the next run. The applied set is built on
	 * top of $P2VI_PATCH, so emptying the variable is what empties the set. */
	unsetenv("P2VI_PATCH");
	if (replay_scripts(sc, 1, 1, -1, -1) != 0) {
		fprintf(stderr, "%s: replay failed, script left alone\n", path);
		ed_free();
		return -1;
	}
	for (i = 0; i < xbufcur; i++)
		if (bufs[i].path && bufs[i].path[0])
			buf_to_diff(out, bufs[i].path, bufs[i].lb);
	ed_free();
	return 0;
}

/* One line of unified diff, from a file, stdin or the built-in differ under -E;
 * consumed in place (chomped, and paths cut out of it). */
static int diff_in_hunk;	/* inside an @@ hunk */
static int diff_old_line;	/* the original line the next op sits at */

/* A parse always begins at a file header, so switching destinations (a compat
 * diff into its own files[] range) only has to clear what a mid-file header
 * would otherwise carry over. */
static void parse_diff_reset(void)
{
	diff_in_hunk = 0;
	diff_old_line = 0;
	pending_is_new = 0;
	free(pending_orig_path);
	pending_orig_path = NULL;
}

/* A "--- "/"+++ " header's path, in place: the a//b/ prefix dropped and the
 * trailing tab/space timestamp cut off. */
static char *diff_path(char *p)
{
	char *t;
	if (p[0] && p[1] == '/')
		p += 2;
	if ((t = strpbrk(p, "\t ")))
		*t = '\0';
	return p;
}

static void parse_diff_line(char *line)
{
	if (strncmp(line, "+++ ", 4) == 0) {
		new_file(diff_path(line + 4));
		diff_in_hunk = 0;
		return;
	}
	/* "--- /dev/null" means the next +++ creates the file; otherwise the
	 * path names the pre-patch content, which on disk is what the script
	 * runs against - the input to file-aware anchor validation. */
	if (strncmp(line, "--- ", 4) == 0) {
		char *p = line + 4;
		pending_is_new = strncmp(p, "/dev/null", 9) == 0
				 && (!p[9] || p[9] == '\t' || p[9] == ' ');
		free(pending_orig_path);
		pending_orig_path = pending_is_new ? NULL
				    : uc_dup(diff_path(p));
		return;
	}
	if (strncmp(line, "diff ", 5) == 0 || strncmp(line, "index ", 6) == 0)
		return;

	int os, oc;
	if (parse_hunk_header(line, &os, &oc)) {
		diff_in_hunk = 1;
		diff_old_line = os;
		cur_hunk_lo = os;
		cur_hunk_hi = oc > 0 ? os + oc - 1 : os;
		/* GNU diff -N marks a created file with the nonexistent
		 * path and an epoch timestamp instead of /dev/null, so
		 * detect it by its sole "@@ -0,0" hunk too; a later hunk
		 * addressing real lines means it existed after all. */
		if (nfiles) {
			if (os == 0 && oc == 0 && files[nfiles - 1].nops == 0)
				files[nfiles - 1].is_new = 1;
			else if (os > 0)
				files[nfiles - 1].is_new = 0;
		}
		return;
	}

	if (!diff_in_hunk || nfiles == 0)
		return;
	if (line[0] == ' ' || line[0] == '-') {
		add_op(line[0] == ' ' ? 'c' : 'd', diff_old_line, line + 1);
		diff_old_line++;
	} else if (line[0] == '+')
		add_op('a', diff_old_line, line + 1);
	else if (line[0] != '\\')   /* not "\ No newline at end of file" */
		diff_in_hunk = 0;
}

/* Feed a whole in-memory unified diff through the line parser. */
static void parse_diff_text(const char *text)
{
	const char *p, *nl;
	sbuf_smake(lb, SB_INIT)
	for (p = text; *p; p = nl + 1) {
		nl = strchr(p, '\n');
		int len = nl ? (int)(nl - p) : (int)strlen(p);
		sbuf_cut(lb, 0)
		sbuf_mem(lb, p, len)
		sbufn_chr(lb, '\n')
		add_raw(lb->s);
		sbufn_cut(lb, len)
		parse_diff_line(lb->s);
		if (!nl)
			break;
	}
	free(lb->s);
}

/*
 * A generated script's tail metadata in one left-to-right pass: every
 * === PATCH2VI COMPAT === region and the === COMPAT PATCH === diff it carries.
 * Regions nest one deep and are fenced by === END COMPAT ===, never by a line
 * count, so a hand-edit that adds or drops a line still parses. Stops at
 * === PATCH2VI PATCH ===, leaving the host diff to the caller. Anything else
 * stored back there is skipped, so a script an older patch2vi wrote reads too.
 */
static int read_stored_sections(FILE *in)
{
	char *line;
	int exit_found = 0;
	sbuf_smake(lb, SB_INIT)
	/* Skip until "exit 0" line; EOF first means the script was cut short
	 * and nothing past the cut can be trusted - refuse rather than
	 * regenerate an empty script over it */
	while ((line = read_line(in, lb))) {
		chomp(line);
		if (strcmp(line, "exit 0") == 0) {
			exit_found = 1;
			break;
		}
	}
	if (!exit_found) {
		free(lb->s);
		fprintf(stderr, "%s: not a patch2vi script (no exit 0)\n",
			input_file ? input_file : "<stdin>");
		return -1;
	}
	/* Compat tail-region state, depth 1: in_compat_patch routes the
	 * block's diff into its own files[] range and raw sink. It is closed by
	 * === END ===, the region by === END COMPAT ===. */
	compat_block_t *cur_cb = NULL;
	int in_compat_patch = 0;
	while (read_line(in, lb)) {
		line = chomp_sb(lb);
		/* === COMPAT PATCH === body: raw diff lines, so a source
		 * line that looks like a section tag is harmless and only
		 * a column-0 === END === closes it. */
		if (in_compat_patch) {
			if (strcmp(line, end_tag_rd) == 0) {
				cur_cb->count = nfiles - cur_cb->first;
				raw_sink = NULL;
				in_compat_patch = 0;
			} else {
				sbufn_chr(lb, '\n')
				add_raw(lb->s);
				sbufn_cut(lb, lb->s_n - 1)
				parse_diff_line(lb->s);
			}
			continue;
		}
		if (strncmp(line, "=== PATCH2VI COMPAT ", 20) == 0) {
			if (cur_cb) {
				fprintf(stderr, "nested COMPAT region\n");
				return -1;
			}
			ARR_PUSH(compat_blocks, ncompat, compat_cap)
			cur_cb = &compat_blocks[ncompat++];
			/* "=== PATCH2VI COMPAT <reg> [src=<origin>...] ==="
			 * One src= field per origin, so the label of a
			 * multi-origin block is everything from the first one
			 * to the terminator, inner "src=" included: it is kept
			 * whole and only ever printed back. */
			char *src = strstr(line + 20, " src=");
			char *e = src ? strstr(src, " ===") : NULL;
			if (e)
				*e = '\0';
			cur_cb->origin = uc_dup(src ? src + 5 : "");
			continue;
		}
		if (cur_cb && strcmp(line, "=== END COMPAT ===") == 0) {
			cur_cb = NULL;
			continue;
		}
		if (cur_cb && strcmp(line, "=== COMPAT PATCH ===") == 0) {
			in_compat_patch = 1;
			raw_sink = &cur_cb->raw;
			parse_diff_reset();
			cur_cb->first = nfiles;
			continue;
		}
		if (strncmp(line, "=== PATCH2VI PATCH ===", 22) == 0) {
			if (cur_cb) {
				fprintf(stderr, "unterminated COMPAT region\n");
				return -1;
			}
			break;
		}
	}
	free(lb->s);
	return 0;
}

/*
 * The applied-set tail.
 *
 * The applied set is the chain of scripts already run, carried in $P2VI_PATCH
 * as basenames. A script inherits it from its caller, hands it to the editor
 * whole (REG_APPLIED, where emit_compat_gates decides every gate from it) and
 * appends itself before invoking the next script with the rest of the queue.
 * The set and the queue are disjoint: the environment grows in run order, the
 * arguments shrink. That is the whole shell side of compat - no header, no
 * flags, nothing to keep in step with the editor's own reading of the set.
 *
 * Only the argument test is a shell conditional; the body lines are ordinary
 * top-level commands, so the -e parser skips the whole block.
 */
static void emit_compat_tail(void)
{
	printf("\nif [ $# -gt 0 ]; then\n");
	printf("    export P2VI_PATCH=\"$P2VI_PATCH ${0##*/}\"\n");
	printf("    next=$1\n");
	printf("    shift\n");
	/* an argument that already carries a path is named as it stands,
	 * wherever ./ cannot reach it; only a bare name, which the shell would
	 * hunt down $PATH, is the one that has to be pinned to this directory */
	printf("    case $next in /*|./*|../*) \"$next\" \"$@\" ;;"
	       " *) \"./$next\" \"$@\" ;; esac\n");
	printf("fi\n");
}

/* The whole help, on stdout for -h (a request, answered with success) and on
 * stderr for a misused option (a diagnostic); err picks both the stream and
 * the exit status. */
static void usage(const char *prog, int err)
{
	FILE *f = err ? stderr : stdout;
	fprintf(f, "Patch2vi-1.0 Usage:\n\n"
		"%s [-arh] [-o FILE] [-er TAG] [-ew TAG] [input.patch]\n"
		"%s -e script.sh [script2.sh...]\n"
		"%s [-ar]I [nextvi-opts...]\n"
		"%s [-aro]E script.sh [reg|''] [nextvi-opts...]\n"
		"%s [-o]C origin.sh [-C origin2.sh...] target.sh"
		" [fix.[patch|sh]|''] [nextvi-opts...]\n\n",
		prog, prog, prog, prog, prog);
	fputs("Converts unified diff to shell script using nextvi ex commands\n"
	      "Input can be a unified diff or a previously generated patch2vi script\n"
	      "  -h    Show this help\n"
	      "  -a    Absolute line numbers\n"
	      "  -r    Relative regex patterns (default)\n"
	      "  -o    Write the script to FILE, atomically; may be a file this\n"
	      "        run reads. Clustered with another option it takes no FILE\n"
	      "        and updates that option's own script in place\n"
	      "  -e    Execute a script with the built-in nextvi, no shell involved\n"
	      "        Several scripts run in order, stopping at the first failure\n", f);
	fprintf(f, "  -er   Read section end tag (default: \"%s\")\n"
		"  -ew   Write section end tag (default: \"%s\")\n",
		end_tag_rd, end_tag_wr);
	fputs("  -E    Update a script: replay it, edit, re-emit its base patch\n"
	      "        Stored compat blocks are carried over from their stored\n"
	      "        patches, unverified\n"
	      "        A compat block's section register after the script rebuilds\n"
	      "        that one block instead, replaying its src= origins ahead of\n"
	      "        the target; '' skips the slot. Rest of the line is a nextvi\n"
	      "        command line\n"
	      "        With QF2=1 the hunks that missed are put back into the\n"
	      "        buffers at the line they reported, cursor parked on the first\n"
	      "  -I    Edit files in the built-in nextvi, emit the edits as a script\n"
	      "        Rest of the line is a nextvi command line, EXINIT included\n"
	      "  -C    Compat patch: resolve a collision with origin.sh, ship the\n"
	      "        fix as a block after the target's, behind an identity gate\n"
	      "        on origin being in $P2VI_PATCH; a second positional\n"
	      "        pre-applies a written fix to start from, '' skips it; the\n"
	      "        rest of the line is a nextvi command line for the handover\n"
	      "        Repeat -C to gate one block on several origins at once\n", f);
	exit(err ? 1 : 0);
}

/* A multi-letter option's argument, attached (-erTAG) or separate (-er TAG);
 * n is where the attached form starts, i.e. one past the option's last
 * letter (3 for -er/-ew, 2 for -C and 3 for the clustered -oC). */
static const char *opt_arg(int argc, char **argv, int *i, int n)
{
	if (argv[*i][n])
		return argv[*i] + n;
	if (*i + 1 < argc)
		return argv[++*i];
	fprintf(stderr, "Option -%.*s requires an argument\n",
		n - 1, argv[*i] + 1);
	usage(argv[0], 1);
	return NULL;
}

/* Is what follows a leading "-o" an option cluster naming -E rather than a
 * file name? Only when it holds that letter and nothing but cluster letters,
 * so that "-oE" (and "-oaE") means "update the script in place" while any
 * ordinary -oFILE, even -oEDITED, still names a file. -E reads a script and
 * emits one, so in place is what an author means. */
static int amend_cluster(const char *s)
{
	int k;
	if (!strchr(s, 'E'))
		return 0;
	for (k = 0; s[k]; k++)
		if (!strchr("arIEo", s[k]))
			return 0;
	return 1;
}

int main(int argc, char **argv)
{
	int i, j;

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (argv[i][1] == '-' && !argv[i][2]) {
			i++;
			break;
		}
		if (argv[i][1] == 'e' && argv[i][2] == 'r') {
			end_tag_rd = opt_arg(argc, argv, &i, 3);
			continue;
		}
		if (argv[i][1] == 'e' && argv[i][2] == 'w') {
			end_tag_wr = opt_arg(argc, argv, &i, 3);
			continue;
		}
		/* -C origin.sh: derive a compatibility patch against that
		 * script, applied AFTER the target (the ordinary positional
		 * input). Post-only.
		 *
		 * Repeatable: every -C adds one origin, and the block's
		 * identity gate tests all of them together, for a fix that only
		 * a particular stack of patches needs. The origins are their
		 * own arguments rather than positionals so the target and the
		 * optional pre-applied fix stay where they are, unambiguously.
		 *
		 * "-oC" clusters the top-level -o into it, as "-oE" does: no
		 * FILE of its own, the result lands back on the target script
		 * the block extends. A file literally named "C" is still
		 * reachable as "-o C". */
		j = argv[i][1] == 'o' && argv[i][2] == 'C';
		if (argv[i][1 + j] == 'C') {
			compat_mode = 1;
			amend_inplace |= j;
			ARR_PUSH(compat_origins, ncompat_origin, compat_origin_cap)
			compat_origins[ncompat_origin++] =
				opt_arg(argc, argv, &i, 2 + j);
			continue;
		}
		/* -o FILE (or -oFILE): the script, wherever it comes from,
		 * lands in that file rather than on stdout; tested after -C
		 * so it cannot shadow it */
		if (argv[i][1] == 'o' && !amend_cluster(argv[i] + 2)) {
			if (argv[i][2])
				out_file = argv[i] + 2;
			else if (i + 1 < argc)
				out_file = argv[++i];
			else {
				fprintf(stderr, "Option -o requires an argument\n");
				usage(argv[0], 1);
			}
			continue;
		}
		/* bare -e: execute the script; tested after -er/-ew so it
		 * cannot shadow them, and kept out of the cluster loop
		 * whose letters are a r h E I */
		if (argv[i][1] == 'e' && !argv[i][2]) {
			exec_mode = 1;
			continue;
		}
		for (j = 1; argv[i][j]; j++) {
			if (argv[i][j] == 'a') {
				relative_mode = 0;
				absolute_opt = 1;
			} else if (argv[i][j] == 'r') {
				relative_mode = 1;
				absolute_opt = 0;
			}
			/* -I and -E both end patch2vi's own option parsing:
			 * whatever follows the cluster is a nextvi command
			 * line, options and files alike - for -E all but its
			 * first word, which names the script to update. Either
			 * way the script goes to stdout, as in every mode */
			else if (argv[i][j] == 'I')
				edit_mode = 1;
			else if (argv[i][j] == 'E')
				amend_mode = 1;
			/* -o inside an -E cluster takes no argument of its
			 * own: the script -E names is also the output, so
			 * -oE updates it in place */
			else if (argv[i][j] == 'o')
				amend_inplace = 1;
			else if (argv[i][j] == 'h')
				usage(argv[0], 0);	/* asked for: stdout, ok */
			else {
				fprintf(stderr, "Unknown option: -%c\n", argv[i][j]);
				usage(argv[0], 1);
			}
		}
		if (edit_mode || amend_mode) {	/* the rest belongs to nextvi */
			i++;
			break;
		}
	}
	if (amend_inplace && !amend_mode && !compat_mode) {
		fprintf(stderr, "Clustered -o is only for -E and -C\n");
		usage(argv[0], 1);
	}
	if (i < argc && !edit_mode)
		input_file = argv[i];
	/* -oC: the block extends the target script, so that is what the run
	 * writes back; the write is atomic, so reading it first is safe */
	if (compat_mode && amend_inplace) {
		if (!input_file) {
			fprintf(stderr, "-oC requires a target script\n");
			return 1;
		}
		out_file = input_file;
	}
	/* plain -C replays the target by name (compat_derive feeds its path
	 * to the same parser -E uses), so stdin cannot supply it */
	if (compat_mode && !amend_inplace && !input_file) {
		fprintf(stderr, "-C requires a target script\n");
		return 1;
	}
	/* -C takes a second positional: an already written compat fix, applied
	 * before the editor is handed over. An empty string skips the slot,
	 * which is how the handover's nextvi command line stays reachable
	 * without naming a fix. The rest parses as -E's does; its files are
	 * opened only at handover, after the baseline snapshot, so they never
	 * leak into the derived diff (the differ skips buffers it has no
	 * baseline for). */
	if (compat_mode && !edit_mode && !amend_mode) {
		char **ha = argv + i + 1;
		int nh = argc - i - 1;
		if (nh > 0) {
			if (ha[0][0])
				compat_pre = ha[0];
			ha++;
			nh--;
		}
		if (parse_hand_args(ha, nh) < 0)
			usage(argv[0], 1);
	}
	/* -E: the first word after the cluster is the script to update, read
	 * like any other input; then an optional block selector, then the
	 * editor's command line.
	 *
	 * The selector is a stored block's section register, the number the
	 * "=== PATCH2VI COMPAT <reg>" header and the "# Compat <reg>" comment
	 * both carry. With it, that one block is what the run rebuilds and
	 * everything else in the script stands; without it -E updates the base
	 * patch as it always did. An empty word skips the slot, which is how
	 * the handover's nextvi command line stays reachable, exactly as -C's
	 * fix slot does. */
	if (amend_mode) {
		char **ha = argv + i + 1;
		int nh = argc - i - 1;
		if (i >= argc) {
			fprintf(stderr, "-E requires a script argument\n");
			return 1;
		}
		if (amend_inplace)
			out_file = argv[i];
		if (nh > 0 && !ha[0][strspn(ha[0], "0123456789")]) {
			if (ha[0][0]) {
				amend_sel = atoi(ha[0]);
				/* a script carrying blocks is search anchored
				 * throughout: an absolute edit next to a gated
				 * section would clobber a line by number in a
				 * tree the gate just reshaped */
				relative_mode = 1;
			}
			ha++;
			nh--;
		}
		if (parse_hand_args(ha, nh) < 0)
			usage(argv[0], 1);
	}
	/* the tail's mode flags govern the loads too, not just the session:
	 * "-m" must already silence the line every ed_loadbuf prints, so they
	 * cannot wait for ed_serve to apply them at entry. The replay's own
	 * per-block "xvis |= 2" stays on top of whatever lands here; ed_serve
	 * resets to the flags verbatim once the bodies are done. */
	if (hand_vis >= 0)
		xvis = hand_vis;

	/* Mark chars that cannot be ex separators. */
	static const char *forbidden =
		" \t0123456789+-.,<>/$';%*#|" /* ex range syntax */
		"@&!?bpaefidgmqrwusxycjtohlv=" /* ex commands */
		":\"\\`\n\r";                  /* default sep, shell quote/escape/backtick, newline */
	for (const char *p = forbidden; *p; p++)
		byte_used[(unsigned char)*p] = 1;

	if (relative_mode || compat_mode)
		mark_bytes_used("FAIL OK");

	/* -I: the diff is not read, it is made. Everything patch2vi's own
	 * option loop did not consume is a nextvi command line - flags after
	 * "--", then files (a missing one counts as a creation) - and the
	 * buffers that session leaves behind are diffed against their disk
	 * copies, that diff going through the parser in place of an input
	 * stream. The script itself goes to stdout, like every other mode. */
	sbuf_smake(dsb, SB_INIT)
	if (edit_mode) {
		if (i >= argc) {
			fprintf(stderr, "-I requires a file argument\n");
			return 1;
		}
		if (edit_to_diff(argv + i, argc - i, dsb) < 0)
			return 1;
		sbuf_nul(dsb)
		parse_diff_text(dsb->s);
	}

	/* -e: no conversion, just run the script through the embedded editor
	 * and report the status the shell would have reported. Every remaining
	 * positional is a script of its own, run in the order given and each in
	 * its own editor lifetime, the first failure stopping the run: what
	 * "./a.sh && ./b.sh" does, without a shell or a process per script. */
	if (exec_mode) {
		if (!input_file) {
			fprintf(stderr, "-e requires a script argument\n");
			return 1;
		}
		int first = i;	/* the first script argument */
		for (; i < argc; i++) {
			/* the scripts already run are this one's applied set, the
			 * chain order -e shares with the shell */
			cur_applied_set((const char **)argv + first, i - first);
			FILE *f = fopen(argv[i], "r");
			if (!f) {
				perror(argv[i]);
				return 1;
			}
			exec_script = argv[i];
			j = exec_p2vi_script(f);
			fclose(f);
			if (j)
				return j;
		}
		cur_applied_free();
		return 0;
	}

	FILE *in = edit_mode ? NULL : stdin;
	if (input_file && !edit_mode) {
		in = fopen(input_file, "r");
		if (!in) {
			perror(input_file);
			return 1;
		}
	}

	/* The first line tells a generated script from a plain patch: the
	 * script's stored regions are read whole, the patch is parsed from
	 * this line on. */
	sbuf_smake(lb, SB_INIT)
	if (in && read_line(in, lb)) {
		if (!strncmp(lb->s, "#!/bin/sh", 9)) {
			if (read_stored_sections(in) < 0)
				return 1;
		} else if (amend_mode) {
			fprintf(stderr, "%s: not a patch2vi script\n", input_file);
			return 1;
		} else {
			/* a patch's first line: keep it and parse it like any other */
			add_raw(lb->s);
			chomp(lb->s);
			parse_diff_line(lb->s);
		}
	}
	/* -E: the stored regions are read, but the old patch section is not -
	 * the new one is what the session produces, over the files as they are
	 * on disk. Close before the loop below reads it. */
	if (amend_mode && amend_sel < 0) {
		if (in)
			fclose(in);
		in = NULL;
		/* The stored compat blocks stand: each is re-emitted from its
		 * own === COMPAT PATCH === with its register, label and
		 * position, exactly as a plain regen re-emits them, while the
		 * base patch is the part this run replaces. That is the layout
		 * a regen already produces - blocks at the head of files[], the
		 * host diff parsed in after them - and emit_one_call numbers
		 * buffers by section, not by files[], so the order costs
		 * nothing. What it cannot do is re-derive them: a block's
		 * anchors were cut against the base as it was, so a base edit
		 * under one of them leaves it missing at run time. Say so, and
		 * leave the verifying to a replay of each src= stack. */
		if (ncompat) {
			fprintf(stderr, "%s: %d compat block%s re-emitted from "
				"stored patches, unverified: a base edit under "
				"one leaves it missing - replay each src= stack "
				"before shipping\n",
				input_file, ncompat, ncompat > 1 ? "s" : "");
			/* the stored blocks re-emit search anchored whatever
			 * the host's mode is, and their fallbacks use the FAIL
			 * OK bytes, so those stay marked even under -a. The
			 * re-derived host patch goes relative too unless -a
			 * asked out: with an origin applied ahead of it the
			 * host body's own lines have moved, and an absolute
			 * edit would clobber one by number. */
			if (!absolute_opt)
				relative_mode = 1;
			else
				fprintf(stderr, "%s: -a keeps the host body "
					"absolute - ship it for the pristine "
					"base only, an origin chained ahead "
					"moves the lines it edits by number\n",
					input_file);
			mark_bytes_used("FAIL OK");
		}
		if (amend_to_diff(input_file, dsb) < 0)
			return 1;
		sbuf_nul(dsb)
		parse_diff_text(dsb->s);
	}
	while (in && read_line(in, lb)) {
		add_raw(lb->s);
		chomp(lb->s);
		parse_diff_line(lb->s);
	}
	free(lb->s);

	if (in && in != stdin)
		fclose(in);

	/* -E reg: the host patch has just been read out of the script and
	 * every other stored region is in hand, so all this replaces is the
	 * named block's own diff. Same window as -C below, one block down.
	 * Every section here emits through the compat window, which is
	 * search anchored on its own; the flag below only decides whether
	 * the env-switch comment block is due. */
	if (amend_mode && amend_sel >= 0) {
		if (amend_derive() < 0)
			return 1;
		if (!absolute_opt)
			relative_mode = 1;
	}

	/* -C: replay the origin script in one session and hand the
	 * tree it leaves behind to the user, who reshapes it so the target
	 * applies. Runs before the separator is picked, so the bytes of
	 * whatever the session produces are seen by find_unused_byte(). */
	if (compat_mode) {
		if (compat_derive() < 0)
			return 1;
		/* The host (target) is regenerated search-anchored too: on a tree
		 * the compat block already merged, its own hunk must fail to find
		 * its anchor and be skipped (QF1 empty), rather than an absolute
		 * edit clobbering a line by number. */
		relative_mode = 1;
	}

	sep = find_unused_byte();
	if (sep < 0) {
		fprintf(stderr,
			"error: patch uses all possible byte values, cannot find separator\n");
		return 1;
	}
	/* Next unused byte becomes the ex escape character; if none is
	 * left, fall back to the default backslash escape paths. */
	byte_used[sep] = 1;
	dyn_esc = find_unused_byte();
	if (dyn_esc < 0)
		dyn_esc = 0;
	else
		byte_used[dyn_esc] = 1;

	/* -o: from here on stdout is the output file's temp twin. Every mode
	 * that emits a script passes through this point, and everything any of
	 * them reads - the patch, the script's stored regions, the files a
	 * replay or an -I session opened - has been read by now, so -o may name
	 * a file the same run consumed (-E updating its own script). */
	if (out_file && out_redirect(out_file) < 0)
		return 1;

	/* Emit shell script header; the emit layer targets sbufs, so build
	 * stdout pieces in one scratch sbuf and flush it after each use */
	sbuf_smake(osb, SB_INIT)
	fputs("#!/bin/sh -e\n# Generated by patch2vi from unified diff\n", stdout);
	list_unused_bytes(osb);
	sbuf_nul(osb)
	fputs(osb->s, stdout);
	fputs("\nVI=${VI:-vi}\n"
	      "if ! $VI -? 2>&1 | grep -q 'Nextvi'; then\n"
	      "    echo \"Error: $VI is not nextvi\" >&2\n"
	      "    echo \"Set VI environment variable to point to nextvi binary\" >&2\n"
	      "    exit 1\n"
	      "fi\n\n", stdout);
	if (relative_mode || compat_mode || ncompat)
		fputs("# Env switches:\n"
		      "# Phase 1 (search/mark) reports nothing by default\n"
		      "#   DBG1=1 reports failures and which fallback anchor\n"
		      "#   resolved a group, QF1=1 also quits on failure\n"
		      "# Phase 2 (edits) reports and quits by default\n"
		      "#   DBG2=1 silences it, QF2=1 keeps going after an error\n"
		      "# INTR=1 enters vi at the failing code line in this\n"
		      "#   script, for state inspection mid execution\n\n", stdout);

	for (int i = 0; i < nfiles; i++)
		build_file_groups(&files[i]);

	/* Host active = files with groups outside every compat block's range;
	 * each compat block's own files are emitted as its own $VI invocation. */
	file_patch_t **active = emalloc((nfiles + 1) * sizeof(*active));
	int nactive = 0;
	for (int i = 0; i < nfiles; i++) {
		int owned = 0;
		if (files[i].ngroups == 0)
			continue;
		for (int c = 0; c < ncompat; c++)
			if (i >= compat_blocks[c].first &&
			    i < compat_blocks[c].first + compat_blocks[c].count)
				owned = 1;
		if (!owned)
			active[nactive++] = &files[i];
	}

	/* With compat blocks present, the whole patch is one $VI call: host and
	 * every compat block share one process so the flags cross the host body
	 * through registers. Without them the common case stays a
	 * single host block, emitted byte-identically as before. */
	if (ncompat) {
		emit_one_call(active, nactive);
	} else if (nactive > 0) {
		/* A large body overflows EXINIT/argv, so the $VI invocation stages
		 * its ex command body in a temp file the shell expands. */
		fputs("# Body too large for EXINIT/argv: stage it in a file\n"
		      "( : > /tmp/p2vi.$$ ) 2>/dev/null && P2VIF=/tmp/p2vi.$$ || P2VIF=./p2vi.$$\n"
		      "trap 'rm -f \"$P2VIF\"' EXIT\n\n", stdout);
		fputs("# Patch:", stdout);
		for (int k = 0; k < nactive; k++)
			fprintf(stdout, " %s", active[k]->path);
		fputc('\n', stdout);
		emit_vi_block(active, nactive);
	}

	/* The chaining tail: if arguments remain, append this script to the
	 * inherited applied set and invoke the next script with the rest. */
	emit_compat_tail();

	/* Embed the compat blocks and the original patch after exit 0 */
	printf("\nexit 0\n");
	emit_compat_storage();
	printf("=== PATCH2VI PATCH ===\n");
	for (int i = 0; i < nraw; i++)
		fputs(raw_lines[i], stdout);

	free(osb->s);
	free(dsb->s);
	/* the script is whole: put it under the name -o asked for */
	if (out_tmp && out_commit(out_file) < 0)
		return 1;
	return 0;
}
