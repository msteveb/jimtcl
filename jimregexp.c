/*
 * regcomp and regexec -- regsub and regerror are elsewhere
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
 *** hoptoad!gnu, on 27 Dec 1986, to add \n as an alternative to |
 *** to assist in implementing egrep.
 *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
 *** hoptoad!gnu, on 27 Dec 1986, to add \< and \> for word-matching
 *** as in BSD grep and ex.
 *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
 *** hoptoad!gnu, on 28 Dec 1986, to optimize characters quoted with \.
 *** THIS IS AN ALTERED VERSION.  It was altered by James A. Woods,
 *** ames!jaw, on 19 June 1987, to quash a regcomp() redundancy.
 *** THIS IS AN ALTERED VERSION.  It was altered by Christopher Seiwald
 *** seiwald@vix.com, on 28 August 1993, for use in jam.  Regmagic.h
 *** was moved into regexp.h, and the include of regexp.h now uses "'s
 *** to avoid conflicting with the system regexp.h.  Const, bless its
 *** soul, was removed so it can compile everywhere.  The declaration
 *** of strchr() was in conflict on AIX, so it was removed (as it is
 *** happily defined in string.h).
 *** THIS IS AN ALTERED VERSION.  It was altered by Christopher Seiwald
 *** seiwald@perforce.com, on 20 January 2000, to use function prototypes.
 *** THIS IS AN ALTERED VERSION.  It was altered by Christopher Seiwald
 *** seiwald@perforce.com, on 05 November 2002, to const string literals.
 *
 *   THIS IS AN ALTERED VERSION.  It was altered by Steve Bennett <steveb@workware.net.au>
 *   on 16 October 2010, to remove static state and add better Tcl ARE compatibility.
 *   This includes counted repetitions, UTF-8 support, character classes,
 *   shorthand character classes, increased number of parentheses to 100,
 *   backslash escape sequences.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "jim.h"
#include "jimregexp.h"
#include "utf8.h"

#if !defined(HAVE_REGCOMP) || defined(JIM_REGEXP)

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* This *MUST* be less than (255-20)/2=117 */
#define REG_MAX_PAREN 100

/* definition	number	opnd?	meaning */
#define	END	0	/* no	End of program. */
#define	BOL	1	/* no	Match "" at beginning of line. */
#define	EOL	2	/* no	Match "" at end of line. */
#define	ANY	3	/* no	Match any one character. */
#define	ANYOF	4	/* str	Match any character in this string. */
#define	ANYBUT	5	/* str	Match any character not in this string. */
#define	BRANCH	6	/* node	Match this alternative, or the next... */
#define	BACK	7	/* no	Match "", "next" ptr points backward. */
#define	EXACTLY	8	/* str	Match this string. */
#define	NOTHING	9	/* no	Match empty string. */
#define	STAR	10	/* node	Match this (simple) thing 0 or more times. */
#define	PLUS	11	/* node	Match this (simple) thing 1 or more times. */
#define	WORDA	12	/* no	Match "" at wordchar, where prev is nonword */
#define	WORDZ	13	/* no	Match "" at nonwordchar, where prev is word */
#define	OPEN	20	/* no	Mark this point in input as start of #n. */
			/*	OPEN+1 is number 1, etc. */
#define	CLOSE	(OPEN+REG_MAX_PAREN)	/* no	Analogous to OPEN. */
#define	CLOSE_END	(CLOSE+REG_MAX_PAREN)

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	REG_MAGIC	0xFADED00D

/*
 * Opcode notes:
 *
 * BRANCH	The set of branches constituting a single choice are hooked
 *		together with their "next" pointers, since precedence prevents
 *		anything being concatenated to any individual branch.  The
 *		"next" pointer of the last BRANCH in a choice points to the
 *		thing following the whole choice.  This is also where the
 *		final "next" pointer of each individual branch points; each
 *		branch starts with the operand node of a BRANCH node.
 *
 * BACK		Normal "next" pointers all implicitly point forward; BACK
 *		exists to make loop structures possible.
 *
 * STAR,PLUS	'?', and complex '*' and '+', are implemented as circular
 *		BRANCH structures using BACK.  Simple cases (one character
 *		per match) are implemented with STAR and PLUS for speed
 *		and to minimize recursive plunges.
 *
 * OPEN,CLOSE	...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */
#define	OP(p)	((p)[0])
#define	NEXT(p)	((p)[1])
#define	OPERAND(p)	((p) + 2)

/*
 * See regmagic.h for one further detail of program structure.
 */


/*
 * Utility definitions.
 */
//#define	UCHARAT(p)	(*(p))

#define	FAIL(R,M)	{ (R)->err = (M); return (M); }
#define	ISMULT(c)	((c) == '*' || (c) == '+' || (c) == '?' || (c) == '{')
#define	META	"^$.[()|?{+*"

/*
 * Flags to be passed up and down.
 */
#define	HASWIDTH	01	/* Known never to match null string. */
#define	SIMPLE		02	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		04	/* Starts with * or +. */
#define	WORST		0	/* Worst case. */

/*
 * Forward declarations for regcomp()'s friends.
 */
static int *reg(regex_t *preg, int paren /* Parenthesized? */, int *flagp );
static int *regpiece(regex_t *preg, int *flagp );
static int *regbranch(regex_t *preg, int *flagp );
static int *regatom(regex_t *preg, int *flagp );
static int *regnode(regex_t *preg, int op );
static const int *regnext(regex_t *preg, const int *p );
static void regc(regex_t *preg, int b );
static int *reginsert(regex_t *preg, int op, int *opnd );
static void regtail(regex_t *preg, int *p, const int *val );
static void regoptail(regex_t *preg, int *p, const int *val );

static int reg_range_find(const int *string, int c, int nocase);
static const char *str_find(const char *string, int c, int nocase);
static int prefix_cmp(const int *prog, int proglen, const char *string, int nocase);

/*#define DEBUG*/
#ifdef DEBUG
int regnarrate = 0;
static void regdump(regex_t *preg);
static const char *regprop( const int *op );
#endif


static int regdummy;

/**
 * Returns the length of the null-terminated integer sequence.
 */
static int str_int_len(const int *seq)
{
	int n = 0;
	while (*seq++) {
		n++;
	}
	return n;
}

/*
 - regcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */
int regcomp(regex_t *preg, const char *exp, int cflags)
{
	const int *scan;
	const int *longest;
	unsigned len;
	int flags;

	memset(preg, 0, sizeof(*preg));

	if (exp == NULL)
		FAIL(preg, REG_ERR_NULL_ARGUMENT);

	/* First pass: determine size, legality. */
	preg->cflags = cflags;
	preg->regparse = exp;
	preg->re_nsub = 0;
	preg->regsize = 0L;
	preg->regcode = &regdummy;
	regc(preg, REG_MAGIC);
	if (reg(preg, 0, &flags) == NULL)
		return preg->err;

	/* Small enough for pointer-storage convention? */
	if (preg->regsize >= 32767L || preg->re_nsub >= REG_MAX_PAREN)		/* Probably could be 65535L. */
		FAIL(preg,REG_ERR_TOO_BIG);

	/* Allocate space. */
	preg->program = malloc(preg->regsize * sizeof(*preg->program));
	if (preg->program == NULL)
		FAIL(preg, REG_ERR_NOMEM);

	/* Second pass: emit code. */
	preg->regparse = exp;
	preg->re_nsub = 0;
	preg->regsize = 0L;
	preg->regcode = preg->program;
	regc(preg, REG_MAGIC);
	if (reg(preg, 0, &flags) == NULL)
		return preg->err;

	/* Dig out information for optimizations. */
	preg->regstart = 0;	/* Worst-case defaults. */
	preg->reganch = 0;
	preg->regmust = NULL;
	preg->regmlen = 0;
	scan = preg->program+1;			/* First BRANCH. */
	if (OP(regnext(preg, scan)) == END) {		/* Only one top-level choice. */
		scan = OPERAND(scan);

		/* Starting-point info. */
		if (OP(scan) == EXACTLY)
			preg->regstart = *OPERAND(scan);
		else if (OP(scan) == BOL)
			preg->reganch++;

		/*
		 * If there's something expensive in the r.e., find the
		 * longest literal string that must appear and make it the
		 * regmust.  Resolve ties in favor of later strings, since
		 * the regstart check works with the beginning of the r.e.
		 * and avoiding duplication strengthens checking.  Not a
		 * strong reason, but sufficient in the absence of others.
		 */
		if (flags&SPSTART) {
			longest = NULL;
			len = 0;
			for (; scan != NULL; scan = regnext(preg, scan)) {
				if (OP(scan) == EXACTLY) {
					int plen = str_int_len(OPERAND(scan));
					if (plen >= len) {
						longest = OPERAND(scan);
						len = plen;
					}
				}
			}
			preg->regmust = longest;
			preg->regmlen = len;
		}
	}

#ifdef DEBUG
	regdump(preg);
#endif

	return 0;
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
static int *reg(regex_t *preg, int paren /* Parenthesized? */, int *flagp )
{
	int *ret;
	int *br;
	const int *ender;
	int parno = 0;
	int flags;

	*flagp = HASWIDTH;	/* Tentatively. */

	/* Make an OPEN node, if parenthesized. */
	if (paren) {
		parno = ++preg->re_nsub;
		ret = regnode(preg, OPEN+parno);
	} else
		ret = NULL;

	/* Pick up the branches, linking them together. */
	br = regbranch(preg, &flags);
	if (br == NULL)
		return(NULL);
	if (ret != NULL)
		regtail(preg, ret, br);	/* OPEN -> first. */
	else
		ret = br;
	if (!(flags&HASWIDTH))
		*flagp &= ~HASWIDTH;
	*flagp |= flags&SPSTART;
	while (*preg->regparse == '|' || *preg->regparse == '\n') {
		preg->regparse++;
		br = regbranch(preg, &flags);
		if (br == NULL)
			return(NULL);
		regtail(preg, ret, br);	/* BRANCH -> BRANCH. */
		if (!(flags&HASWIDTH))
			*flagp &= ~HASWIDTH;
		*flagp |= flags&SPSTART;
	}

	/* Make a closing node, and hook it on the end. */
	ender = regnode(preg, (paren) ? CLOSE+parno : END);	
	regtail(preg, ret, ender);

	/* Hook the tails of the branches to the closing node. */
	for (br = ret; br != NULL; br = (int *)regnext(preg, br))
		regoptail(preg, br, ender);

	/* Check for proper termination. */
	if (paren && *preg->regparse++ != ')') {
		preg->err = REG_ERR_UNMATCHED_PAREN;
		return NULL;
	} else if (!paren && *preg->regparse != '\0') {
		if (*preg->regparse == ')') {
			preg->err = REG_ERR_UNMATCHED_PAREN;
			return NULL;
		} else {
			preg->err = REG_ERR_JUNK_ON_END;
			return NULL;
		}
	}

	return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
static int *regbranch(regex_t *preg, int *flagp )
{
	int *ret;
	int *chain;
	int *latest;
	int flags;

	*flagp = WORST;		/* Tentatively. */

	ret = regnode(preg, BRANCH);
	chain = NULL;
	while (*preg->regparse != '\0' && *preg->regparse != ')' &&
	       *preg->regparse != '\n' && *preg->regparse != '|') {
		latest = regpiece(preg, &flags);
		if (latest == NULL)
			return(NULL);
		*flagp |= flags&HASWIDTH;
		if (chain == NULL) {/* First piece. */
			*flagp |= flags&SPSTART;
		}
		else {
			regtail(preg, chain, latest);
		}
		chain = latest;
	}
	if (chain == NULL)	/* Loop ran zero times. */
		(void) regnode(preg, NOTHING);

	return(ret);
}

/**
 * Duplicates the program at 'pos' of length 'len' at the end of the program.
 * 
 * If 'maketail' is set, the next point for 'pos' is set to skip to the next
 * part of the program after 'pos'.
 */
static int *regdup(regex_t *preg, int *pos, int len, int maketail)
{
	int i;

	preg->regsize += len;

	if (preg->regcode == &regdummy) {
		return pos;
	}

	for (i = 0; i < len; i++) {
		regc(preg, pos[i]);
	}
	if (maketail) {
		regtail(preg, pos, pos + len);
	}
	return preg->regcode - len;
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static int *regpiece(regex_t *preg, int *flagp)
{
	int *ret;
	char op;
	int *next;
	int flags;
	int size = preg->regsize;
	int *chain = NULL;

	ret = regatom(preg, &flags);
	if (ret == NULL)
		return(NULL);

	size = preg->regsize - size;

	op = *preg->regparse;
	if (!ISMULT(op)) {
		*flagp = flags;
		return(ret);
	}

	if (!(flags&HASWIDTH) && op != '?') {
		preg->err = REG_ERR_OPERAND_COULD_BE_EMPTY;
		return NULL;
	}
	*flagp = (op != '+') ? (WORST|SPSTART) : (WORST|HASWIDTH);

	/* Handle braces (counted repetition) by expansion */
	if (op == '{') {
		int min = 0;
		int max = 0;
		char *end;

		min = strtoul(preg->regparse + 1, &end, 10);
		if (end == preg->regparse + 1) {
			if (*end == ',') {
				min = 0;
			}
			else {
				preg->err = REG_ERR_BAD_COUNT;
				return NULL;
			}
		}
		preg->regparse = end;
		max = strtoul(preg->regparse + 1, &end, 10);
		if (*end != '}') {
			preg->err = REG_ERR_UNMATCHED_BRACES;
			return NULL;
		}
		if (end == preg->regparse + 1) {
			max = -1;
		}
		else if (max < min || max >= 100) {
			preg->err = REG_ERR_BAD_COUNT;
			return NULL;
		}
		if (min >= 100) {
			preg->err = REG_ERR_BAD_COUNT;
			return NULL;
		}

		preg->regparse = strchr(preg->regparse, '}');

		/* By default, chain to the start of the sequence */
		chain = ret;

		if (max < 0 || max == min) {
			/* Simple case */
			if (max == min) {
				if (min == 0) {
					/* {0,0} so do nothing at all */
					reginsert(preg, NOTHING, ret);
					preg->regparse++;
					return ret;
				}
				/* Output 'min - 1' instances of 'x' */
				min--;
				op = 0;
			}
			else {
				/* {n,} is just xxxx* */
				op = '*';
				/* No - chain to the tail of the sequence */
				chain = NULL;
			}

			/* We need to duplicate the arg 'min' times */
			while (min--) {
				ret = regdup(preg, ret, size, 1);
			}
		}
		else {
			/* Complex case */
			int i;

			/* Chaining is needed */

			/* Need to emit some min args first */
			for (i = 0; i < min; i++) {
				ret = regdup(preg, ret, size, 1);
			}

			for (i = min; i < max; i++) {
				/* Emit x */
				/* There is already one instance of 'reg' at the end */
				/* Add another 'reg' at the end */
				int *prog;

				/* Convert to (x|), just like ? */
				prog = reginsert(preg, BRANCH, ret);			/* Either x */
				regtail(preg, ret, regnode(preg, BRANCH));		/* or */
				next = regnode(preg, NOTHING);		/* null. */
				regtail(preg, ret, next);
				regoptail(preg, ret, next);

				/* Now grab a copy ready for the next iteration */
				if (i != max - 1) {
					ret = regdup(preg, prog, size, 0);
				}
			}
			op = 0;
		}
	}

	if (op == '*' && (flags&SIMPLE))
		reginsert(preg, STAR, ret);
	else if (op == '*') {
		/* Emit x* as (x&|), where & means "self". */
		reginsert(preg, BRANCH, ret);			/* Either x */
		regoptail(preg, ret, regnode(preg, BACK));		/* and loop */
		regoptail(preg, ret, ret);			/* back */
		regtail(preg, ret, regnode(preg, BRANCH));		/* or */
		regtail(preg, ret, regnode(preg, NOTHING));		/* null. */
	} else if (op == '+' && (flags&SIMPLE))
		reginsert(preg, PLUS, ret);
	else if (op == '+') {
		/* Emit x+ as x(&|), where & means "self". */
		next = regnode(preg, BRANCH);			/* Either */
		regtail(preg, ret, next);
		regtail(preg, regnode(preg, BACK), ret);		/* loop back */
		regtail(preg, next, regnode(preg, BRANCH));		/* or */
		regtail(preg, ret, regnode(preg, NOTHING));		/* null. */
	} else if (op == '?') {
		/* Emit x? as (x|) */
		reginsert(preg, BRANCH, ret);			/* Either x */
		regtail(preg, ret, regnode(preg, BRANCH));		/* or */
		next = regnode(preg, NOTHING);		/* null. */
		regtail(preg, ret, next);
		regoptail(preg, ret, next);
	}
	preg->regparse++;
	if (ISMULT(*preg->regparse)) {
		preg->err = REG_ERR_NESTED_COUNT;
		return NULL;
	}

	return chain ? chain : ret;
}

/**
 * Add all characters in the inclusive range between lower and upper.
 * 
 * Handles a swapped range (upper < lower).
 */
static void reg_addrange(regex_t *preg, int lower, int upper)
{
	if (lower > upper) {
		reg_addrange(preg, upper, lower);
	}
	/* Add a range as length, start */
	regc(preg, upper - lower + 1);
	regc(preg, lower);
}

/**
 * Add a null-terminated literal string as a set of ranges.
 */
static void reg_addrange_str(regex_t *preg, const char *str)
{
	while (*str) {
		reg_addrange(preg, *str, *str);
		str++;
	}
}

/**
 * Extracts the next unicode char from utf8.
 * 
 * If 'upper' is set, converts the char to uppercase.
 */
static int utf8_tounicode_case(const char *s, int *uc, int upper)
{
	int l = utf8_tounicode(s, uc);
	if (upper) {
		*uc = utf8_upper(*uc);
	}
	return l;
}

/**
 * Converts a hex digit to decimal.
 * 
 * Returns -1 for an invalid hex digit.
 */
static int xdigitval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/**
 * Parses up to 'n' hex digits at 's' and stores the result in *uc.
 *
 * Returns the number of hex digits parsed.
 * If there are no hex digits, returns 0 and stores nothing.
 */
static int parse_hex(const char *s, int n, int *uc)
{
	int val = 0;
	int k;

	for (k = 0; k < n; k++) {
		int c = xdigitval(*s++);
		if (c == -1) {
			break;
		}
		val = (val << 4) | c;
	}
	if (k) {
		*uc = val;
	}
	return k;
}

/**
 * Call for chars after a backlash to decode the escape sequence.
 * 
 * Stores the result in *ch.
 *
 * Returns the number of bytes consumed.
 */
static int reg_decode_escape(const char *s, int *ch)
{
	int n;
	const char *s0 = s;

	*ch = *s++;

	switch (*ch) {
		case 'b': *ch = '\b'; break;
		case 'e': *ch = 27; break;
		case 'f': *ch = '\f'; break;
		case 'n': *ch = '\n'; break;
		case 'r': *ch = '\r'; break;
		case 't': *ch = '\t'; break;
		case 'v': *ch = '\v'; break;
		case 'u':
			if ((n = parse_hex(s, 4, ch)) > 0) {
				s += n;
			}
			break;
		case 'x':
			if ((n = parse_hex(s, 2, ch)) > 0) {
				s += n;
			}
			break;
		case '\0':
			s--;
			*ch = '\\';
			break;
	}
	return s - s0;
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
static int *regatom(regex_t *preg, int *flagp)
{
	int *ret;
	int flags;
	int nocase = (preg->cflags & REG_ICASE);

	int ch;
	int n = utf8_tounicode_case(preg->regparse, &ch, nocase);

	*flagp = WORST;		/* Tentatively. */

	preg->regparse += n;
	switch (ch) {
	/* FIXME: these chars only have meaning at beg/end of pat? */
	case '^':
		ret = regnode(preg, BOL);
		break;
	case '$':
		ret = regnode(preg, EOL);
		break;
	case '.':
		ret = regnode(preg, ANY);
		*flagp |= HASWIDTH|SIMPLE;
		break;
	case '[': {
			const char *pattern = preg->regparse;

			if (*pattern == '^') {	/* Complement of range. */
				ret = regnode(preg, ANYBUT);
				pattern++;
			} else
				ret = regnode(preg, ANYOF);

			/* Special case. If the first char is ']' or '-', it is part of the set */
			if (*pattern == ']' || *pattern == '-') {
				reg_addrange(preg, *pattern, *pattern);
				pattern++;
			}

			while (*pattern && *pattern != ']') {
				/* Is this a range? a-z */
				int start;
				int end;

				pattern += utf8_tounicode_case(pattern, &start, nocase);
				if (start == '\\') {
					pattern += reg_decode_escape(pattern, &start);
					if (start == 0) {
						preg->err = REG_ERR_NULL_CHAR;
						return NULL;
					}
				}
				if (pattern[0] == '-' && pattern[1]) {
					/* skip '-' */
					pattern += utf8_tounicode(pattern, &end);
					pattern += utf8_tounicode_case(pattern, &end, nocase);
					if (end == '\\') {
						pattern += reg_decode_escape(pattern, &end);
						if (end == 0) {
							preg->err = REG_ERR_NULL_CHAR;
							return NULL;
						}
					}

					reg_addrange(preg, start, end);
					continue;
				}
				if (start == '[') {
					if (strncmp(pattern, ":alpha:]", 8) == 0) {
						if ((preg->cflags & REG_ICASE) == 0) {
							reg_addrange(preg, 'a', 'z');
						}
						reg_addrange(preg, 'A', 'Z');
						pattern += 8;
						continue;
					}
					if (strncmp(pattern, ":alnum:]", 8) == 0) {
						if ((preg->cflags & REG_ICASE) == 0) {
							reg_addrange(preg, 'a', 'z');
						}
						reg_addrange(preg, 'A', 'Z');
						reg_addrange(preg, '0', '9');
						pattern += 8;
						continue;
					}
					if (strncmp(pattern, ":space:]", 8) == 0) {
						reg_addrange_str(preg, " \t\r\n\f\v");
						pattern += 8;
						continue;
					}
				}
				/* Not a range, so just add the char */
				reg_addrange(preg, start, start);
			}
			regc(preg, '\0');

			if (*pattern) {
				pattern++;
			}
			preg->regparse = pattern;

			*flagp |= HASWIDTH|SIMPLE;
		}
		break;
	case '(':
		ret = reg(preg, 1, &flags);
		if (ret == NULL)
			return(NULL);
		*flagp |= flags&(HASWIDTH|SPSTART);
		break;
	case '\0':
	case '|':
	case '\n':
	case ')':
		preg->err = REG_ERR_INTERNAL;
		return NULL;	/* Supposed to be caught earlier. */
	case '?':
	case '+':
	case '*':
	case '{':
		preg->err = REG_ERR_COUNT_FOLLOWS_NOTHING;
		return NULL;
	case '\\':
		switch (*preg->regparse++) {
		case '\0':
			preg->err = REG_ERR_TRAILING_BACKSLASH;
			return NULL;
			break;
		case '<':
		case 'm':
			ret = regnode(preg, WORDA);
			break;
		case '>':
		case 'M':
			ret = regnode(preg, WORDZ);
			break;
		case 'd':
			ret = regnode(preg, ANYOF);
			reg_addrange(preg, '0', '9');
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		case 'w':
			ret = regnode(preg, ANYOF);
			if ((preg->cflags & REG_ICASE) == 0) {
				reg_addrange(preg, 'a', 'z');
			}
			reg_addrange(preg, 'A', 'Z');
			reg_addrange(preg, '0', '9');
			reg_addrange(preg, '_', '_');
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		case 's':
			ret = regnode(preg, ANYOF);
			reg_addrange_str(preg," \t\r\n\f\v");
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		/* FIXME: Someday handle \1, \2, ... */
		default:
			/* Handle general quoted chars in exact-match routine */
			/* Back up to include the backslash */
			preg->regparse--;
			goto de_fault;
		}
		break;
	de_fault:
	default: {
			/*
			 * Encode a string of characters to be matched exactly.
			 */
			int added = 0;

			/* Back up to pick up the first char of interest */
			preg->regparse -= n;

			ret = regnode(preg, EXACTLY);

			/* Note that a META operator such as ? or * consumes the
			 * preceding char.
			 * Thus we must be careful to look ahead by 2 and add the
			 * last char as it's own EXACTLY if necessary
			 */

			/* Until end of string or a META char is reached */
			while (*preg->regparse && strchr(META, *preg->regparse) == NULL) {
				n = utf8_tounicode_case(preg->regparse, &ch, (preg->cflags & REG_ICASE));
				if (ch == '\\' && preg->regparse[n]) {
					/* Non-trailing backslash.
					 * Is this a special escape, or a regular escape?
					 */
					if (strchr("<>mMwds", preg->regparse[n])) {
						/* A special escape. All done with EXACTLY */
						break;
					}
					/* Decode it. Note that we add the length for the escape
					 * sequence to the length for the backlash so we can skip
					 * the entire sequence, or not as required.
					 */
					n += reg_decode_escape(preg->regparse + n, &ch);
					if (ch == 0) {
						preg->err = REG_ERR_NULL_CHAR;
						return NULL;
					}
				}

				/* Now we have one char 'ch' of length 'n'.
				 * Check to see if the following char is a MULT
				 */

				if (ISMULT(preg->regparse[n])) {
					/* Yes. But do we already have some EXACTLY chars? */
					if (added) {
						/* Yes, so return what we have and pick up the current char next time around */
						break;
					}
					/* No, so add this single char and finish */
					regc(preg, ch);
					added++;
					preg->regparse += n;
					break;
				}

				/* No, so just add this char normally */
				regc(preg, ch);
				added++;
				preg->regparse += n;
			}
			regc(preg, '\0');

			*flagp |= HASWIDTH;
			if (added == 1)
				*flagp |= SIMPLE;
			break;
		}
		break;
	}

	return(ret);
}

/*
 - regnode - emit a node
 */
/* Location. */
static int *regnode(regex_t *preg, int op)
{
	int *ret;
	int *ptr;

	preg->regsize += 2;
	ret = preg->regcode;
	if (ret == &regdummy) {
		return(ret);
	}

	ptr = ret;
	*ptr++ = op;
	*ptr++ = 0;		/* Null "next" pointer. */
	preg->regcode = ptr;

	return(ret);
}

/*
 - regc - emit (if appropriate) a byte of code
 */
static void regc(regex_t *preg, int b )
{
	preg->regsize++;
	if (preg->regcode != &regdummy)
		*preg->regcode++ = b;
}

/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 * Returns the new location of the original operand.
 */
static int *reginsert(regex_t *preg, int op, int *opnd )
{
	int *src;
	int *dst;
	int *place;

	preg->regsize += 2;

	if (preg->regcode == &regdummy) {
		return opnd;
	}

	src = preg->regcode;
	preg->regcode += 2;
	dst = preg->regcode;
	while (src > opnd)
		*--dst = *--src;

	place = opnd;		/* Op node, where operand used to be. */
	*place++ = op;
	*place++ = 0;

	return place;
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void regtail(regex_t *preg, int *p, const int *val )
{
	int *scan;
	int *temp;
	int offset;

	if (p == &regdummy)
		return;

	/* Find last node. */
	scan = p;
	for (;;) {
		temp = (int *)regnext(preg, scan);
		if (temp == NULL)
			break;
		scan = temp;
	}

	if (OP(scan) == BACK)
		offset = scan - val;
	else
		offset = val - scan;

	scan[1] = offset;
}

/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */

static void regoptail(regex_t *preg, int *p, const int *val )
{
	/* "Operandless" and "op != BRANCH" are synonymous in practice. */
	if (p == NULL || p == &regdummy || OP(p) != BRANCH)
		return;
	regtail(preg, OPERAND(p), val);
}

/*
 * regexec and friends
 */

/*
 * Forwards.
 */
static int regtry(regex_t *preg, const char *string );
static int regmatch(regex_t *preg, const int *prog);
static int regrepeat(regex_t *preg, const int *p );

/*
 - regexec - match a regexp against a string
 */
int regexec(regex_t  *preg,  const  char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
	const char *s;

	/* Be paranoid... */
	if (preg == NULL || preg->program == NULL || string == NULL) {
		return REG_ERR_NULL_ARGUMENT;
	}

	/* Check validity of program. */
	if (*preg->program != REG_MAGIC) {
		return REG_ERR_CORRUPTED;
	}

#ifdef DEBUG
	/*regdump(preg);*/
#endif

	preg->eflags = eflags;
	preg->pmatch = pmatch;
	preg->nmatch = nmatch;
	preg->start = string;	/* All offsets are computed from here */

	/* If there is a "must appear" string, look for it. */
	if (preg->regmust != NULL) {
		s = string;
		while ((s = str_find(s, preg->regmust[0], preg->cflags & REG_ICASE)) != NULL) {
			if (prefix_cmp(preg->regmust, preg->regmlen, s, preg->cflags & REG_ICASE) >= 0) {
				break;
			}
			s++;
		}
		if (s == NULL)	/* Not present. */
			return REG_NOMATCH;
	}

	/* Mark beginning of line for ^ . */
	preg->regbol = string;

	/* Simplest case:  anchored match need be tried only once (maybe per line). */
	if (preg->reganch) {
		if (eflags & REG_NOTBOL) {
			/* This is an anchored search, but not an BOL, so possibly skip to the next line */
			goto nextline;
		}
		while (1) {
			int ret = regtry(preg, string);
			if (ret) {
				return REG_NOERROR;
			}
			if (*string) {
nextline:
				if (preg->cflags & REG_NEWLINE) {
					/* Try the next anchor? */
					string = strchr(string, '\n');
					if (string) {
						preg->regbol = ++string;
						continue;
					}
				}
			}
			return REG_NOMATCH;
		}
	}

	/* Messy cases:  unanchored match. */
	s = string;
	if (preg->regstart != '\0') {
		/* We know what char it must start with. */
		while ((s = str_find(s, preg->regstart, preg->cflags & REG_ICASE)) != NULL) {
			if (regtry(preg, s))
				return REG_NOERROR;
			s++;
		}
	}
	else
		/* We don't -- general case. */
		do {
			if (regtry(preg, s))
				return REG_NOERROR;
		} while (*s++ != '\0');

	/* Failure. */
	return REG_NOMATCH;
}

/*
 - regtry - try match at specific point
 */
			/* 0 failure, 1 success */
static int regtry( regex_t *preg, const char *string )
{
	int i;

	preg->reginput = string;

	for (i = 0; i < preg->nmatch; i++) {
		preg->pmatch[i].rm_so = -1;
		preg->pmatch[i].rm_eo = -1;
	}
	if (regmatch(preg, preg->program + 1)) {
		preg->pmatch[0].rm_so = string - preg->start;
		preg->pmatch[0].rm_eo = preg->reginput - preg->start;
		return(1);
	} else
		return(0);
}

/**
 * Returns bytes matched if 'pattern' is a prefix of 'string'.
 *
 * If 'nocase' is non-zero, does a case-insensitive match.
 *
 * Returns -1 on not found.
 */
static int prefix_cmp(const int *prog, int proglen, const char *string, int nocase)
{
	const char *s = string;
	while (proglen && *s) {
		int ch;
		int n = utf8_tounicode_case(s, &ch, nocase);
		if (ch != *prog) {
			return -1;
		}
		prog++;
		s += n;
		proglen--;
	}
	if (proglen == 0) {
		return s - string;
	}
	return -1;
}

/**
 * Searchs for 'c' in the range 'range'.
 * 
 * If 'nocase' is set, the range is assumed to be uppercase
 * and 'c' is converted to uppercase before matching.
 *
 * Returns 1 if found, or 0 if not.
 */
static int reg_range_find(const int *range, int c, int nocase)
{
	if (nocase) {
		/* The "string" should already be converted to uppercase */
		c = utf8_upper(c);
	}
	while (*range) {
		if (c >= range[1] && c <= (range[0] + range[1] - 1)) {
			return 1;
		}
		range += 2;
	}
	return 0;
}

/**
 * Search for the character 'c' in the utf-8 string 'string'.
 * 
 * If 'nocase' is set, the 'string' is assumed to be uppercase
 * and 'c' is converted to uppercase before matching.
 *
 * Returns the byte position in the string where the 'c' was found, or
 * NULL if not found.
 */
static const char *str_find(const char *string, int c, int nocase)
{
	if (nocase) {
		/* The "string" should already be converted to uppercase */
		c = utf8_upper(c);
	}
	while (*string) {
		int ch;
		int n = utf8_tounicode_case(string, &ch, nocase);
		if (c == ch) {
			return string;
		}
		string += n;
	}
	return NULL;
}

/**
 * Returns true if 'ch' is an end-of-line char.
 * 
 * In REG_NEWLINE mode, \n is considered EOL in
 * addition to \0
 */
static int reg_iseol(regex_t *preg, int ch)
{
	if (preg->cflags & REG_NEWLINE) {
		return ch == '\0' || ch == '\n';
	}
	else {
		return ch == '\0';
	}
}

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
/* 0 failure, 1 success */
static int regmatch(regex_t *preg, const int *prog)
{
	const int *scan;	/* Current node. */
	const int *next;		/* Next node. */

	scan = prog;
#ifdef DEBUG
	if (scan != NULL && regnarrate)
		fprintf(stderr, "%s(\n", regprop(scan));
#endif
	while (scan != NULL) {
#ifdef DEBUG
		if (regnarrate)
			fprintf(stderr, "%s...\n", regprop(scan));
#endif
		next = regnext(preg, scan);

		switch (OP(scan)) {
		case BOL:
			if (preg->reginput != preg->regbol)
				return(0);
			break;
		case EOL:
			if (!reg_iseol(preg, *preg->reginput)) {
				return(0);
			}
			break;
		case WORDA:
			/* Must be looking at a letter, digit, or _ */
			if ((!isalnum(*preg->reginput)) && *preg->reginput != '_')
				return(0);
			/* Prev must be BOL or nonword */
			if (preg->reginput > preg->regbol &&
			    (isalnum(preg->reginput[-1]) || preg->reginput[-1] == '_'))
				return(0);
			break;
		case WORDZ:
			/* Must be looking at non letter, digit, or _ */
			if (isalnum(*preg->reginput) || *preg->reginput == '_')
				return(0);
			/* We don't care what the previous char was */
			break;
		case ANY:
			if (reg_iseol(preg, *preg->reginput))
				return 0;
			preg->reginput++;
			break;
		case EXACTLY: {
				const int *opnd;
				int len;
				int slen;

				opnd = OPERAND(scan);
				len = str_int_len(opnd);

				slen = prefix_cmp(opnd, len, preg->reginput, preg->cflags & REG_ICASE);
				if (slen < 0) {
					return(0);
				}
				preg->reginput += slen;
			}
			break;
		case ANYOF:
			if (reg_iseol(preg, *preg->reginput))
				return 0;
			if (reg_range_find(OPERAND(scan), *preg->reginput, preg->cflags & REG_ICASE) == 0)
				return(0);
			preg->reginput++;
			break;
		case ANYBUT:
			if (reg_iseol(preg, *preg->reginput))
				return 0;
			if (reg_range_find(OPERAND(scan), *preg->reginput, preg->cflags & REG_ICASE) != 0)
				return(0);
			preg->reginput++;
			break;
		case NOTHING:
			break;
		case BACK:
			break;
		case BRANCH: {
				const char *save;

				if (OP(next) != BRANCH)		/* No choice. */
					next = OPERAND(scan);	/* Avoid recursion. */
				else {
					do {
						save = preg->reginput;
						if (regmatch(preg, OPERAND(scan)))
							return(1);
						preg->reginput = save;
						scan = regnext(preg, scan);
					} while (scan != NULL && OP(scan) == BRANCH);
					return(0);
					/* NOTREACHED */
				}
			}
			break;
		case STAR:
		case PLUS: {
				char nextch;
				int no;
				const char *save;
				int min;

				/*
				 * Lookahead to avoid useless match attempts
				 * when we know what character comes next.
				 */
				nextch = '\0';
				if (OP(next) == EXACTLY)
					nextch = *OPERAND(next);
				min = (OP(scan) == STAR) ? 0 : 1;
				save = preg->reginput;
				no = regrepeat(preg, OPERAND(scan));
				while (no >= min) {
					int ch;
					utf8_tounicode_case(preg->reginput, &ch, (preg->cflags & REG_ICASE));
					/* If it could work, try it. */
					if (reg_iseol(preg, nextch) || ch == nextch)
						if (regmatch(preg, next))
							return(1);
					/* Couldn't or didn't -- back up. */
					no--;
					preg->reginput = save + no;
				}
				return(0);
			}
			break;
		case END:
			return(1);	/* Success! */
			break;
		default:
			if (OP(scan) >= OPEN+1 && OP(scan) < CLOSE_END) {
				const char *save;

				save = preg->reginput;

				if (regmatch(preg, next)) {
					int no;
					/*
					 * Don't set startp if some later
					 * invocation of the same parentheses
					 * already has.
					 */
					if (OP(scan) < CLOSE) {
						no = OP(scan) - OPEN;
						if (no < preg->nmatch && preg->pmatch[no].rm_so == -1) {
							preg->pmatch[no].rm_so = save - preg->start;
						}
					}
					else {
						no = OP(scan) - CLOSE;
						if (no < preg->nmatch && preg->pmatch[no].rm_eo == -1) {
							preg->pmatch[no].rm_eo = save - preg->start;
						}
					}
					return(1);
				} else
					return(0);
			}
			return REG_ERR_INTERNAL;
		}

		scan = next;
	}

	/*
	 * We get here only if there's trouble -- normally "case END" is
	 * the terminating point.
	 */
	return REG_ERR_INTERNAL;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int regrepeat(regex_t *preg, const int *p )
{
	int count = 0;
	const char *scan;
	const int *opnd;

	scan = preg->reginput;
	opnd = OPERAND(p);
	switch (OP(p)) {
	case ANY:
		while (!reg_iseol(preg, *scan)) {
			count++;
			scan++;
		}
		break;
	case EXACTLY:
		if (preg->cflags & REG_ICASE) {
			while (1) {
				int ch;
				int n = utf8_tounicode_case(scan, &ch, 1);
				if (*opnd != ch) {
					break;
				}
				count++;
				scan += n;
			}
		}
		else {
			while (*opnd == *scan) {
				count++;
				scan++;
			}
		}
		break;
	case ANYOF:
		while (!reg_iseol(preg, *scan) && reg_range_find(opnd, *scan, preg->cflags & REG_ICASE) != 0) {
			count++;
			scan++;
		}
		break;
	case ANYBUT:
		while (!reg_iseol(preg, *scan) && reg_range_find(opnd, *scan, preg->cflags & REG_ICASE) == 0) {
			count++;
			scan++;
		}
		break;
	default:		/* Oh dear.  Called inappropriately. */
		preg->err = REG_ERR_INTERNAL;
		count = 0;	/* Best compromise. */
		break;
	}
	preg->reginput = scan;

	return(count);
}

/*
 - regnext - dig the "next" pointer out of a node
 */
static const int *regnext(regex_t *preg, const int *p )
{
	int offset;

	if (p == &regdummy)
		return(NULL);

	offset = NEXT(p);
	if (offset == 0)
		return(NULL);

	if (OP(p) == BACK)
		return(p-offset);
	else
		return(p+offset);
}

#ifdef DEBUG

/*
 - regdump - dump a regexp onto stdout in vaguely comprehensible form
 */
static void regdump(regex_t *preg)
{
	const int *s;
	char op = EXACTLY;	/* Arbitrary non-END op. */
	const int *next;
	char buf[4];

	if (preg->regcode == &regdummy)
		return;

	s = preg->program + 1;
	while (op != END && s < preg->regcode) {	/* While that wasn't END last time... */
		op = OP(s);
		printf("%2d{%02x}%s", (int)(s-preg->program), op, regprop(s));	/* Where, what. */
		next = regnext(preg, s);
		if (next == NULL)		/* Next ptr. */
			printf("(0)");
		else 
			printf("(%d)", (int)((s-preg->program)+(next-s)));
		s += 2;
		if (op == ANYOF || op == ANYBUT) {
			/* set of ranges */

			while (*s) {
				int len = *s++;
				int first = *s++;
				buf[utf8_fromunicode(buf, first)] = 0;
				printf("%s", buf);
				if (len > 1) {
					buf[utf8_fromunicode(buf, first + len - 1)] = 0;
					printf("-%s", buf);
				}
			}
			s++;
		}
		else if (op == EXACTLY) {
			/* Literal string, where present. */

			while (*s) {
				buf[utf8_fromunicode(buf, *s)] = 0;
				printf("%s", buf);
				s++;
			}
			s++;
		}
		putchar('\n');
	}

	if (op == END) {
		/* Header fields of interest. */
		if (preg->regstart != '\0')
			buf[utf8_fromunicode(buf, preg->regstart)] = 0;
			printf("start '%s' ", buf);
		if (preg->reganch)
			printf("anchored ");
		if (preg->regmust != NULL) {
			int i;
			printf("must have:");
			for (i = 0; i < preg->regmlen; i++) {
				putchar(preg->regmust[i]);
			}
			putchar('\n');
		}
	}
	printf("\n");
}

/*
 - regprop - printable representation of opcode
 */
static const char *regprop( const int *op )
{
	char *p;
	static char buf[50];

	(void) strcpy(buf, ":");

	switch (OP(op)) {
	case BOL:
		p = "BOL";
		break;
	case EOL:
		p = "EOL";
		break;
	case ANY:
		p = "ANY";
		break;
	case ANYOF:
		p = "ANYOF";
		break;
	case ANYBUT:
		p = "ANYBUT";
		break;
	case BRANCH:
		p = "BRANCH";
		break;
	case EXACTLY:
		p = "EXACTLY";
		break;
	case NOTHING:
		p = "NOTHING";
		break;
	case BACK:
		p = "BACK";
		break;
	case END:
		p = "END";
		break;
	case STAR:
		p = "STAR";
		break;
	case PLUS:
		p = "PLUS";
		break;
	case WORDA:
		p = "WORDA";
		break;
	case WORDZ:
		p = "WORDZ";
		break;
	default:
		if (OP(op) >= OPEN && OP(op) < CLOSE) {
			sprintf(buf+strlen(buf), "OPEN%d", OP(op)-OPEN);
		}
		else if (OP(op) >= CLOSE && OP(op) < CLOSE_END) {
			sprintf(buf+strlen(buf), "CLOSE%d", OP(op)-CLOSE);
		}
		else {
			abort();
		}
		p = NULL;
		break;
	}
	if (p != NULL)
		(void) strcat(buf, p);
	return(buf);
}
#endif

size_t regerror(int errcode, const regex_t *preg, char *errbuf,  size_t errbuf_size)
{
	static const char *error_strings[] = {
		"success",
		"no match",
		"bad pattern",
		"null argument",
		"unknown error",
		"too big",
		"out of memory",
		"too many ()",
		"parentheses () not balanced",
		"braces {} not balanced",
		"invalid repetition count(s)",
		"extra characters",
		"*+ of empty atom",
		"nested count",
		"internal error",
		"count follows nothing",
		"trailing backslash",
		"corrupted program",
		"contains null char",
	};
	const char *err;

	if (errcode < 0 || errcode >= REG_ERR_NUM) {
		err = "Bad error code";
	}
	else {
		err = error_strings[errcode];
	}

	return snprintf(errbuf, errbuf_size, "%s", err);
}

void regfree(regex_t *preg)
{
	free(preg->program);
}

#endif
