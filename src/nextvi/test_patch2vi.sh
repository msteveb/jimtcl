#!/bin/sh
# Tests for patch2vi relative mode: f> searches and s/ substitutes
# Run from the nextvi source directory
set -e

# The generated scripts' phase switches are read from the environment (a
# replay resolves ${DBG1:+...} etc through getenv too), so an exported one
# silently rewrites what every test runs. INTR=1 is the worst of them: an
# error site opens the script in vi and a pty test then waits for a
# keystroke forever. Start from the documented defaults.
unset DBG1 DBG2 QF1 QF2 INTR

VI=${VI:-/bin/vi}
PASS=0
FAIL=0
TMPDIR=$(mktemp -d)

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

ok() { PASS=$((PASS + 1)); printf "  PASS: %s\n" "$1"; }
fail() { FAIL=$((FAIL + 1)); printf "  FAIL: %s\n" "$1"; }

# script(1) flavors disagree: busybox knows only [-afq] [-t[FILE]] [-c PROG]
# [OUTFILE], util-linux adds -e (exit with the child's status). No test here
# reads the child's status through script, so keep to the flags both accept
# and swallow script's own status so set -e cannot trip over it. The
# environment goes through env(1) rather than a prefix assignment, which
# would linger in the shell after a function call.
pty() {	# pty <var=value> <shell-command>: run the command on a pty
	env "$1" script -q -c "$2" /dev/null || true
}

check() {
	# $1=test name $2=orig $3=expected $4=patch2vi flags (default: -r)
	local name="$1" flags="${4:--r}"
	printf '%s' "$2" > "$TMPDIR/orig.txt"
	printf '%s' "$3" > "$TMPDIR/expected.txt"
	diff -u "$TMPDIR/orig.txt" "$TMPDIR/expected.txt" > "$TMPDIR/test.patch" || true
	./patch2vi $flags "$TMPDIR/test.patch" > "$TMPDIR/apply.sh" 2>&1
	chmod +x "$TMPDIR/apply.sh"
	cp "$TMPDIR/orig.txt" "$TMPDIR/result.txt"
	# Redirect $VI -e arg from expected.txt to result.txt
	sed -i "s|\$VI -e '[^']*'|\$VI -e '$TMPDIR/result.txt'|" "$TMPDIR/apply.sh"
	# same script without a shell (-e), on its own copy: both paths must
	# produce the same bytes and the same status
	cp "$TMPDIR/orig.txt" "$TMPDIR/result2.txt"
	sed "s|\$VI -e '[^']*'|\$VI -e '$TMPDIR/result2.txt'|" \
		"$TMPDIR/apply.sh" > "$TMPDIR/apply2.sh"
	local sh_rc=0 e_rc=0
	VI="$VI" "$TMPDIR/apply.sh" >/dev/null 2>&1 || sh_rc=$?
	./patch2vi -e "$TMPDIR/apply2.sh" >/dev/null 2>&1 || e_rc=$?
	if [ $sh_rc -ne 0 ] ||
	   ! diff -q "$TMPDIR/result.txt" "$TMPDIR/expected.txt" >/dev/null 2>&1; then
		fail "$name"
		echo "    --- expected ---"
		cat "$TMPDIR/expected.txt" | sed 's/^/    /'
		echo "    --- got ---"
		cat "$TMPDIR/result.txt" | sed 's/^/    /'
	elif [ "$e_rc" != "$sh_rc" ] ||
	     ! diff -q "$TMPDIR/result2.txt" "$TMPDIR/result.txt" >/dev/null 2>&1; then
		fail "$name (-e diverges: status $e_rc vs $sh_rc)"
		echo "    --- sh ---"
		cat "$TMPDIR/result.txt" | sed 's/^/    /'
		echo "    --- -e ---"
		cat "$TMPDIR/result2.txt" | sed 's/^/    /'
	else
		ok "$name"
	fi
}

# Also verify script content (no >pattern> anchors, uses f>)
check_script() {
	# $1=test name $2=orig $3=expected $4=pattern to find $5=pattern to reject
	local name="$1"
	printf '%s' "$2" > "$TMPDIR/orig.txt"
	printf '%s' "$3" > "$TMPDIR/expected.txt"
	diff -u "$TMPDIR/orig.txt" "$TMPDIR/expected.txt" > "$TMPDIR/test.patch" || true
	./patch2vi -r "$TMPDIR/test.patch" > "$TMPDIR/apply.sh" 2>&1
	local found=1 rejected=0
	# the patterns are about the emitted body, and the $VI line carries a
	# mktemp path: one ending in "s" put an "s/" in the script and failed a
	# rejection test at random
	grep -v '\$VI -e' "$TMPDIR/apply.sh" > "$TMPDIR/apply.body"
	if [ -n "$4" ] && ! grep -q "$4" "$TMPDIR/apply.body"; then found=0; fi
	if [ -n "$5" ] && grep -q "$5" "$TMPDIR/apply.body"; then rejected=1; fi
	if [ $found -eq 1 ] && [ $rejected -eq 0 ]; then
		ok "$name"
	else
		fail "$name"
		if [ $found -eq 0 ]; then echo "    expected pattern '$4' not found"; fi
		if [ $rejected -eq 1 ]; then echo "    rejected pattern '$5' found"; fi
		grep 'EXINIT' "$TMPDIR/apply.sh" | sed 's/^/    /'
	fi
}

# Generate a script from orig->expected, then apply it to a *drifted* file to
# confirm the substitute progression (exact -> grp-absorbing)
# absorbs drift.
# $1=name $2=orig $3=expected(script source) $4=drifted input $5=drifted result
check_drift() {
	local name="$1"
	printf '%s' "$2" > "$TMPDIR/orig.txt"
	printf '%s' "$3" > "$TMPDIR/expected.txt"
	diff -u "$TMPDIR/orig.txt" "$TMPDIR/expected.txt" > "$TMPDIR/test.patch" || true
	./patch2vi -r "$TMPDIR/test.patch" > "$TMPDIR/apply.sh" 2>&1
	chmod +x "$TMPDIR/apply.sh"
	printf '%s' "$4" > "$TMPDIR/result.txt"
	printf '%s' "$5" > "$TMPDIR/drifted.txt"
	sed -i "s|\$VI -e '[^']*'|\$VI -e '$TMPDIR/result.txt'|" "$TMPDIR/apply.sh"
	printf '%s' "$4" > "$TMPDIR/result2.txt"
	sed "s|\$VI -e '[^']*'|\$VI -e '$TMPDIR/result2.txt'|" \
		"$TMPDIR/apply.sh" > "$TMPDIR/apply2.sh"
	local sh_rc=0 e_rc=0
	VI="$VI" "$TMPDIR/apply.sh" >/dev/null 2>&1 || sh_rc=$?
	./patch2vi -e "$TMPDIR/apply2.sh" >/dev/null 2>&1 || e_rc=$?
	if [ $sh_rc -ne 0 ] ||
	   ! diff -q "$TMPDIR/result.txt" "$TMPDIR/drifted.txt" >/dev/null 2>&1; then
		fail "$name"
		echo "    --- want ---"; sed 's/^/    /' "$TMPDIR/drifted.txt"
		echo "    --- got ---"; sed 's/^/    /' "$TMPDIR/result.txt"
	elif [ "$e_rc" != "$sh_rc" ] ||
	     ! diff -q "$TMPDIR/result2.txt" "$TMPDIR/result.txt" >/dev/null 2>&1; then
		fail "$name (-e diverges: status $e_rc vs $sh_rc)"
		echo "    --- sh ---"; sed 's/^/    /' "$TMPDIR/result.txt"
		echo "    --- -e ---"; sed 's/^/    /' "$TMPDIR/result2.txt"
	else
		ok "$name"
	fi
}

# build via the script: it renames vi.c's main() around the compile
./build_patch2vi.sh clean >/dev/null
./build_patch2vi.sh build >/dev/null

echo "=== Script content tests ==="

# A one-line file leaves a single deduped search pattern; anything with
# context emits a multi-pattern fallback chain (%f>) instead.
check_script "single pattern uses .,\$f>" \
	"old
" \
	"new
" \
	'\.,$f>' '>[^$]*>'

# A single anchored change emits a fallback chain. The whole-hunk
# pattern is multi-line (%f>, mode 0); the single-line fallbacks
# (top context, deleted line) default to mode 1 and search the live
# buffer with .,$f> instead. The chain opens the ? conditional with a
# line-break no-op before the first search (each attempt starts
# on its own source line); emit_search (single pattern) never opens
# with '?', so this stays chain-specific.
check_script "anchored change uses fallback chain" \
	"ctx1
old
end
" \
	"ctx1
new
end
" \
	'?..0?$' ''

check_script "single-line chain fallbacks use .,\$f>" \
	"ctx1
old
end
" \
	"ctx1
new
end
" \
	'\.,$f>' ''

check_script "multiline anchor uses %f>" \
	"ctx1
ctx2
ctx3
old
end
" \
	"ctx1
ctx2
ctx3
new
end
" \
	'%f>' ''

check_script "no backstep in output" \
	"ctx1
old1
ctx2
old2
end
" \
	"ctx1
new1
ctx2
new2
end
" \
	'' '[.][-]1'

check_script "substitute for horizontal edit" \
	"ctx1
ctx2
ctx3
hello old world
end
" \
	"ctx1
ctx2
ctx3
hello new world
end
" \
	's/' ';[0-9][0-9]*c '

check_script "searches reuse register cache" \
	"aaa
old1
bbb
old2
end
" \
	"aaa
new1
bbb
new2
end
" \
	'%ya 98' '\\$;f>'

echo ""
echo "=== End-to-end apply tests (relative mode) ==="

check "simple change" \
	"line1
line2
line3
old
line5
" \
	"line1
line2
line3
new
line5
"

check "horizontal substitute" \
	"line1
line2
line3
the old value here
line5
" \
	"line1
line2
line3
the new value here
line5
"

check "multiple groups" \
	"aaa
old1
bbb
ccc
ddd
old2
eee
" \
	"aaa
new1
bbb
ccc
ddd
new2
eee
"

check "pure delete" \
	"keep1
keep2
delete_me
keep3
keep4
" \
	"keep1
keep2
keep3
keep4
"

check "pure add" \
	"keep1
keep2
keep3
" \
	"keep1
keep2
added
keep3
"

check "multi-line delete" \
	"ctx1
ctx2
ctx3
del1
del2
del3
end
" \
	"ctx1
ctx2
ctx3
end
"

check "multi-line add" \
	"ctx1
ctx2
ctx3
end
" \
	"ctx1
ctx2
ctx3
add1
add2
add3
end
"

check "change with more adds than dels" \
	"ctx1
ctx2
ctx3
old1
end
" \
	"ctx1
ctx2
ctx3
new1
new2
new3
end
"

check "change with more dels than adds" \
	"ctx1
ctx2
ctx3
old1
old2
old3
end
" \
	"ctx1
ctx2
ctx3
new1
end
"

check "regex metacharacters in substitute" \
	"ctx1
ctx2
ctx3
foo.*bar+baz
end
" \
	"ctx1
ctx2
ctx3
foo.*qux+baz
end
"

check "backslash in content" \
	"ctx1
ctx2
ctx3
path\\old\\value
end
" \
	"ctx1
ctx2
ctx3
path\\new\\value
end
"

check "slash in substitute" \
	"ctx1
ctx2
ctx3
path/to/old
end
" \
	"ctx1
ctx2
ctx3
path/to/new
end
"

check "dollar sign in content" \
	"ctx1
ctx2
ctx3
price \$old here
end
" \
	"ctx1
ctx2
ctx3
price \$new here
end
"

check "follow context (no preceding ctx)" \
	"old
follow1
follow2
end
" \
	"new
follow1
follow2
end
"

check "single context line anchor" \
	"ctx1
old
end
" \
	"ctx1
new
end
"

echo ""
echo "=== Substitute uniqueness expansion tests ==="

# When a substitute pattern appears multiple times on a line, patch2vi
# should expand the diff region with surrounding context until it's unique.

check "duplicate substring: change second foo" \
	"ctx1
ctx2
ctx3
foo bar foo baz
end
" \
	"ctx1
ctx2
ctx3
foo bar qux baz
end
"

check "triple duplicate: change middle occurrence" \
	"ctx1
ctx2
ctx3
aa foo bb foo cc foo dd
end
" \
	"ctx1
ctx2
ctx3
aa foo bb qux cc foo dd
end
"

check "adjacent duplicates" \
	"ctx1
ctx2
ctx3
abab cdcd abab
end
" \
	"ctx1
ctx2
ctx3
abab cdcd efef
end
"

check "duplicate with regex metacharacters" \
	"ctx1
ctx2
ctx3
a.b+c a.b+c end
end
" \
	"ctx1
ctx2
ctx3
a.b+c a.x+c end
end
"

check_script "expansion includes context for uniqueness" \
	"ctx1
ctx2
ctx3
foo bar foo baz
end
" \
	"ctx1
ctx2
ctx3
foo bar qux baz
end
" \
	's/' ''

# When the diff covers the entire old line (can't make unique), should
# fall back to full-line change instead of substitute
check "full-line fallback when substring not unique" \
	"ctx1
ctx2
ctx3
abab
end
" \
	"ctx1
ctx2
ctx3
acab
end
"

check "duplicate at start of line" \
	"ctx1
ctx2
ctx3
xx yy xx zz
end
" \
	"ctx1
ctx2
ctx3
xx yy QQ zz
end
"

check "duplicate at end of line" \
	"ctx1
ctx2
ctx3
aa xx bb xx
end
" \
	"ctx1
ctx2
ctx3
aa QQ bb xx
end
"

check "UTF-8 duplicate expansion" \
	"ctx1
ctx2
ctx3
café bon café fin
end
" \
	"ctx1
ctx2
ctx3
café bon thé fin
end
"

echo ""
echo "=== Grp-capture absorbing substitute tests ==="

# Two changed spots with a stable island between. The progression emits, in
# order: exact (rung 0, the minimal contiguous span) and one grp rung built over
# that SAME span -- the island is wildcarded so it absorbs drift, with no leading
# or trailing "(.*)" (those would just dup the unanchored exact rung). Both edit
# separators X/Y are unique, so the island is a full "(.*)": s/X(.*)Y/P\1Q/.
check_script "two-spot change emits exact rung first" \
	"top
aaaaaaaaaa X bbbbbbbbbb Y cccccccccc
bot
" \
	"top
aaaaaaaaaa P bbbbbbbbbb Q cccccccccc
bot
" \
	's/X bbbbbbbbbb Y/' ''
check_script "two-spot change emits absorbing grp rung" \
	"top
aaaaaaaaaa X bbbbbbbbbb Y cccccccccc
bot
" \
	"top
aaaaaaaaaa P bbbbbbbbbb Q cccccccccc
bot
" \
	'X([.][*])Y' '([.][*])X'

# Same shape applies correctly end-to-end (island preserved verbatim).
check "two-spot grp substitute applies" \
	"top
aaaaaaaaaa X bbbbbbbbbb Y cccccccccc
bot
" \
	"top
aaaaaaaaaa P bbbbbbbbbb Q cccccccccc
bot
"

# Inserting characters between stable anchors (the canonical case). The span is
# "int vi_cn" -> "aint vib_cnc"; the "int vi" run fuzzes to "(in.*vi)" (minimal
# unique head "in" / tail "vi") so it absorbs drift between them, while "_cn" is
# too short and stays literal. No leading/trailing "(.*)": s/(in.*vi)(_cn)/a\1b\2c/.
check_script "insertion between anchors emits absorbing grp" \
	"static int vi_cndir = 1;
" \
	"static aint vib_cncdir = 1;
" \
	'(in[.][*]vi)(_cn)' '([.][*])(int'
check "insertion between anchors applies" \
	"static int vi_cndir = 1;
" \
	"static aint vib_cncdir = 1;
"

# Regression: two insertions bracketing a stable middle. The span is built over
# the changed region only; the unchanged prefix and the short trailing run "|\\"
# are NOT wrapped in "(.*)" absorbers (that would dup the unanchored exact rung).
# Earlier this emitted an external "(.*)...(.*)" form; now it must not -- the grp
# is just the span "(f!.*c!?|)" with insertions injected. The line must be long
# enough that the cost model picks STRAT_REL (s///) over STRAT_RELC (;c).
check_script "short trailing anchor emits span-only grp" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[f!]?!?|f[-+><tdp]?|inc|i|sc!?|\\
line five stays
line six stays
line seven stays
" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[qf!]?!?|f[-+><tdp]?|inc|i|sc!?|vs|sp|\\
line five stays
line six stays
line seven stays
" \
	'(f![.][*]' '([.][*])(f'
check "short trailing anchor grp applies" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[f!]?!?|f[-+><tdp]?|inc|i|sc!?|\\
line five stays
line six stays
line seven stays
" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[qf!]?!?|f[-+><tdp]?|inc|i|sc!?|vs|sp|\\
line five stays
line six stays
line seven stays
"

# The middle anchor is flanked by insertions on both sides ("q" before, "vs|sp"
# after), so it cannot become a full "(.*)" (ambiguous). The grp instead keeps
# the MINIMAL head/tail runes that are each unique in the old line (here head
# "f!", tail "c!?|") and wildcards the middle -- "(f!.*c!?|)" -- which absorbs
# drift *inside* the stable region ("inc" -> "incZZ" on disk) while still
# injecting the insertions. The exact rung fails on the drifted interior; only
# the fuzz grp fires. The "f!.*" marker checks a literal-then-wildcard group.
check_script "insertion-flanked middle emits fuzz grp" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[f!]?!?|f[-+><tdp]?|inc|i|sc!?|\\
line five stays
line six stays
line seven stays
" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[qf!]?!?|f[-+><tdp]?|inc|i|sc!?|vs|sp|\\
line five stays
line six stays
line seven stays
" \
	'f![.][*]' ''
check_drift "fuzz grp absorbs interior drift" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[f!]?!?|f[-+><tdp]?|inc|i|sc!?|\\
line five stays
line six stays
line seven stays
" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[qf!]?!?|f[-+><tdp]?|inc|i|sc!?|vs|sp|\\
line five stays
line six stays
line seven stays
" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[f!]?!?|f[-+><tdp]?|incZZ|i|sc!?|\\
line five stays
line six stays
line seven stays
" \
	"line one stays
line two stays
line three stays
|[@&!dmj]|=\\\\?{0,1}|\\\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[qf!]?!?|f[-+><tdp]?|incZZ|i|sc!?|vs|sp|\\
line five stays
line six stays
line seven stays
"

# The progression absorbs drift: the exact rung fails when the on-disk island
# drifted, but the grp rung's interior "(.*)" absorbs it.
check_drift "grp rung absorbs island drift" \
	"top
aaaaaaaaaa X bbbbbbbbbb Y cccccccccc
bot
" \
	"top
aaaaaaaaaa P bbbbbbbbbb Q cccccccccc
bot
" \
	"top
aaaaaaaaaa X bbZZbbZZbb Y cccccccccc
bot
" \
	"top
aaaaaaaaaa P bbZZbbZZbb Q cccccccccc
bot
"

# A full "(.*)" interior is only safe when both bordering edit separators are
# unique in the old line. When the separator repeats (here both edits are "Z"),
# the bare "Z(.*)Z" split would be ambiguous, so the middle is demoted to a fuzz
# "( b.*b )" whose literal head/tail pin the repeated "Z": s/Z( b.*b )Z/P\1Q/.
# The ambiguous middle wild "Z(.*)Z" must NOT appear.
check_script "repeated separator demotes wild middle" \
	"c1
c2
c3
aaaaaaaaaa Z bbbbbbbbbb Z cccccccccc
c5
c6
c7
" \
	"c1
c2
c3
aaaaaaaaaa P bbbbbbbbbb Q cccccccccc
c5
c6
c7
" \
	'Z( b' 'Z([.][*])Z'
check "repeated separator grp applies" \
	"c1
c2
c3
aaaaaaaaaa Z bbbbbbbbbb Z cccccccccc
c5
c6
c7
" \
	"c1
c2
c3
aaaaaaaaaa P bbbbbbbbbb Q cccccccccc
c5
c6
c7
"

echo ""
echo "=== Pure insertion substitute tests ==="

# When a line change is a pure insertion (old_text empty), patch2vi must
# expand to include surrounding context so s// never has an empty pattern.

check "pure insertion at end of args" \
	"ctx1
ctx2
ctx3
static void led_redraw(char *cs, int r, int orow, int crow, int ctop, int flg)
end
" \
	"ctx1
ctx2
ctx3
static void led_redraw(char *cs, int r, int orow, int crow, int ctop, int flg, int ai_max)
end
"

check_script "pure insertion produces non-empty s/ pattern" \
	"ctx1
ctx2
ctx3
static void led_redraw(char *cs, int r, int orow, int crow, int ctop, int flg)
end
" \
	"ctx1
ctx2
ctx3
static void led_redraw(char *cs, int r, int orow, int crow, int ctop, int flg, int ai_max)
end
" \
	's/.' 's//'

check "pure insertion at start of line" \
	"ctx1
ctx2
ctx3
world
end
" \
	"ctx1
ctx2
ctx3
hello world
end
"

check "pure insertion in middle" \
	"ctx1
ctx2
ctx3
foo bar
end
" \
	"ctx1
ctx2
ctx3
foo baz bar
end
"

check "empty old line changed to non-empty" \
	"ctx1
ctx2
ctx3

end
" \
	"ctx1
ctx2
ctx3
something
end
"

check_script "empty old line uses c not s/" \
	"ctx1
ctx2
ctx3

end
" \
	"ctx1
ctx2
ctx3
something
end
" \
	'c ' 's/'

echo ""
echo "=== Consecutive substitute positioning tests ==="

# When two substitutes are emitted in a row, the second must have
# proper positioning. Previously empty context lines between changes
# caused has_rel/emit_rel_pos mismatch: has_rel=1 but no output.

check "consecutive substitutes with empty line between" \
	"ctx1
ctx2
ctx3
hello old1 world

goodbye old2 world
end
" \
	"ctx1
ctx2
ctx3
hello new1 world

goodbye new2 world
end
"

check "consecutive substitutes no context between" \
	"ctx1
ctx2
ctx3
the old1 value
the old2 value
end
" \
	"ctx1
ctx2
ctx3
the new1 value
the new2 value
end
"

echo ""
echo "=== Insert before first line tests ==="

check "insert before line 1 (relative mode)" \
	"first line
second line
third line
" \
	"new header
first line
second line
third line
" \
	"-r"

echo ""
echo "=== -I edit-to-script tests ==="

# -I and -E open the built-in nextvi on /dev/tty, so these run patch2vi under
# script(1)'s pty. The session is driven entirely by EXINIT (nextvi's own
# startup) or P2VI_EX (patch2vi's test hook, run once the buffers are loaded):
# edits pipe a buffer through an awk filter with ":%!" and quit with "q!".
# No keystrokes are sent, so the run is non-interactive and exits as fast
# as it starts.
if command -v script >/dev/null 2>&1; then

# -I edits a file in the built-in editor and turns the buffer it leaves
# behind into the script; the file itself is never written, so the diff
# comes from patch2vi's own differ, not from disk.
E_P2VI="$PWD/patch2vi"

# run_I <workdir> <excmds> <patch2vi args...>; the session is nextvi's own
# main(), so the ex commands are driven through EXINIT like in vi(1)
run_I() {
	local d="$1" ex="$2"
	shift 2
	pty "EXINIT=$ex" \
		"sh -c 'cd $d && $E_P2VI -I $*'" >/dev/null 2>&1
}

mkdir -p "$TMPDIR/E1"
printf 'one\ntwo\nthree\nfour\nfive\nsix\n' > "$TMPDIR/E1/f.txt"
cp "$TMPDIR/E1/f.txt" "$TMPDIR/E1/orig.txt"
printf 'one\ntwo\nTHREE\nfour\nfive\nsix\n' > "$TMPDIR/E1/want.txt"
printf '/^three$/ { print "THREE"; next }\n{ print }\n' > "$TMPDIR/E1/filt.awk"
run_I "$TMPDIR/E1" '%!awk -f filt.awk:q!' 'f.txt > out.sh'

# the script goes to stdout, the edited file stays untouched
if [ -s "$TMPDIR/E1/out.sh" ] &&
   diff -q "$TMPDIR/E1/f.txt" "$TMPDIR/E1/orig.txt" >/dev/null 2>&1; then
	ok "-I writes a script to stdout, not the file"
else
	fail "-I writes a script to stdout, not the file"
fi

# and applying it reproduces the edit
( cd "$TMPDIR/E1" && VI="$VI" sh out.sh ) >/dev/null 2>&1
if diff -q "$TMPDIR/E1/f.txt" "$TMPDIR/E1/want.txt" >/dev/null 2>&1; then
	ok "-I script applies the edit"
else
	fail "-I script applies the edit"
fi

# the embedded patch is a plain unified diff, byte for byte diff(1)'s
awk '/^=== PATCH2VI PATCH ===$/ { f = 1; next } f' "$TMPDIR/E1/out.sh" |
	tail -n +3 > "$TMPDIR/E1/mine.diff"
diff -u "$TMPDIR/E1/orig.txt" "$TMPDIR/E1/want.txt" |
	tail -n +3 > "$TMPDIR/E1/gnu.diff"
if diff -q "$TMPDIR/E1/gnu.diff" "$TMPDIR/E1/mine.diff" >/dev/null 2>&1; then
	ok "-I diff matches diff -u"
else
	fail "-I diff matches diff -u"
	diff "$TMPDIR/E1/gnu.diff" "$TMPDIR/E1/mine.diff" | sed 's/^/    /'
fi

# every positional argument is a file to open, in order: b0 is the first
# one, the rest are reachable with :b without a single :e
mkdir -p "$TMPDIR/E2"
printf 'p1\np2\n' > "$TMPDIR/E2/p.txt"
printf 'q1\nq2\n' > "$TMPDIR/E2/q.txt"
printf 'r1\nr2\n' > "$TMPDIR/E2/r.txt"
run_I "$TMPDIR/E2" '%s/p1/P1/:b1:%s/q2/Q2/:b2:%s/r1/R1/:q!' \
	'p.txt q.txt r.txt p.txt > out.sh'
if grep -q '^# Patch: p.txt q.txt r.txt$' "$TMPDIR/E2/out.sh"; then
	ok "-I opens every file named on the command line, once each"
else
	fail "-I opens every file named on the command line, once each"
	grep '^# Patch:' "$TMPDIR/E2/out.sh" | sed 's/^/    /'
fi
( cd "$TMPDIR/E2" && VI="$VI" sh out.sh ) >/dev/null 2>&1
printf 'P1\np2\n' > "$TMPDIR/E2/want_p.txt"
printf 'q1\nQ2\n' > "$TMPDIR/E2/want_q.txt"
printf 'R1\nr2\n' > "$TMPDIR/E2/want_r.txt"
if diff -q "$TMPDIR/E2/p.txt" "$TMPDIR/E2/want_p.txt" >/dev/null 2>&1 &&
   diff -q "$TMPDIR/E2/q.txt" "$TMPDIR/E2/want_q.txt" >/dev/null 2>&1 &&
   diff -q "$TMPDIR/E2/r.txt" "$TMPDIR/E2/want_r.txt" >/dev/null 2>&1; then
	ok "-I multi-file script applies to every file"
else
	fail "-I multi-file script applies to every file"
fi

# an unchanged buffer has nothing to convert
mkdir -p "$TMPDIR/E4"
cp "$TMPDIR/E1/orig.txt" "$TMPDIR/E4/f.txt"
run_I "$TMPDIR/E4" 'q' 'f.txt > out.sh'
if [ -s "$TMPDIR/E4/out.sh" ] &&
   ! grep -q '^# Patch:' "$TMPDIR/E4/out.sh"; then
	ok "-I on an untouched buffer emits no patch"
else
	fail "-I on an untouched buffer emits no patch"
fi

# a file that does not exist yet is a creation: /dev/null on the left,
# and -I still leaves the filesystem alone
mkdir -p "$TMPDIR/E3"
printf 'hello\nworld\n' > "$TMPDIR/E3/content.txt"
run_I "$TMPDIR/E3" 'r content.txt:q!' 'new.txt > out.sh'
if [ ! -e "$TMPDIR/E3/new.txt" ] &&
   grep -q '^--- /dev/null$' "$TMPDIR/E3/out.sh"; then
	ok "-I diffs a missing file as a creation"
else
	fail "-I diffs a missing file as a creation"
fi
( cd "$TMPDIR/E3" && VI="$VI" sh out.sh ) >/dev/null 2>&1
if diff -q "$TMPDIR/E3/new.txt" "$TMPDIR/E3/content.txt" >/dev/null 2>&1; then
	ok "-I creation script creates the file"
else
	fail "-I creation script creates the file"
fi

# every buffer of the session lands in one script: two more files reached
# with :e, one of them new, all diffed against disk in the order opened
mkdir -p "$TMPDIR/E5"
printf 'a1\na2\na3\n' > "$TMPDIR/E5/a.txt"
printf 'b1\nb2\nb3\n' > "$TMPDIR/E5/b.txt"
run_I "$TMPDIR/E5" \
	'%s/a2/A2/:e b.txt:%s/b3/B3/:e c.txt:r a.txt:q!' 'a.txt > out.sh'
if grep -q '^# Patch: a.txt b.txt c.txt$' "$TMPDIR/E5/out.sh" &&
   [ ! -e "$TMPDIR/E5/c.txt" ]; then
	ok "-I collects every session buffer"
else
	fail "-I collects every session buffer"
	grep '^# Patch:' "$TMPDIR/E5/out.sh" | sed 's/^/    /'
fi
( cd "$TMPDIR/E5" && VI="$VI" sh out.sh ) >/dev/null 2>&1
printf 'a1\nA2\na3\n' > "$TMPDIR/E5/want_a.txt"
printf 'b1\nb2\nB3\n' > "$TMPDIR/E5/want_b.txt"
# c.txt got a.txt read off disk, that is before the buffer's own edit
printf 'a1\na2\na3\n' > "$TMPDIR/E5/want_c.txt"
if diff -q "$TMPDIR/E5/a.txt" "$TMPDIR/E5/want_a.txt" >/dev/null 2>&1 &&
   diff -q "$TMPDIR/E5/b.txt" "$TMPDIR/E5/want_b.txt" >/dev/null 2>&1 &&
   diff -q "$TMPDIR/E5/c.txt" "$TMPDIR/E5/want_c.txt" >/dev/null 2>&1; then
	ok "-I multi-buffer script applies to every file"
else
	fail "-I multi-buffer script applies to every file"
fi

# buffers left untouched contribute nothing, changed ones still do
mkdir -p "$TMPDIR/E6"
printf 'x1\nx2\n' > "$TMPDIR/E6/x.txt"
printf 'y1\ny2\n' > "$TMPDIR/E6/y.txt"
run_I "$TMPDIR/E6" ':e y.txt:%s/y1/Y1/:q!' 'x.txt > out.sh'
if grep -q '^# Patch: y.txt$' "$TMPDIR/E6/out.sh"; then
	ok "-I skips buffers with no edits"
else
	fail "-I skips buffers with no edits"
	grep '^# Patch:' "$TMPDIR/E6/out.sh" | sed 's/^/    /'
fi

# everything after -I is nextvi's own command line: -e runs the session in
# ex mode, where EXINIT does the editing and there is no vi loop at all
mkdir -p "$TMPDIR/E7"
printf 'z1\nz2\n' > "$TMPDIR/E7/z.txt"
printf 'Z1\nz2\n' > "$TMPDIR/E7/want_z.txt"
run_I "$TMPDIR/E7" '%s/z1/Z1/:q!' '-e z.txt > out.sh'
( cd "$TMPDIR/E7" && VI="$VI" sh out.sh ) >/dev/null 2>&1
if diff -q "$TMPDIR/E7/z.txt" "$TMPDIR/E7/want_z.txt" >/dev/null 2>&1; then
	ok "-I passes nextvi flags straight through"
else
	fail "-I passes nextvi flags straight through"
fi

echo ""
echo "=== -E script-update tests ==="

# -E replays a generated script into one session, hands it over, and emits
# the updated script: the old script's own effect plus what the user did,
# diffed against the files on disk. The replay never writes, so the tree
# the new script is measured against is the untouched one.
A="$TMPDIR/Am"
mkdir -p "$A"

# run_A <workdir> <P2VI_EX> <patch2vi args...>; the handover is driven by
# P2VI_EX, since a replayed session is not nextvi's own main()
run_A() {
	local d="$1" ex="$2"
	shift 2
	pty "P2VI_EX=$ex" \
		"sh -c 'cd $d && $E_P2VI $*'" >/dev/null 2>&1
}

printf 'L1\nL2\nL3\n' > "$A/base.txt"
printf -- '--- a/f.txt\n+++ b/f.txt\n@@ -1,3 +1,3 @@\n L1\n L2\n-L3\n+L3x\n' \
	> "$A/x.diff"
cp "$A/base.txt" "$A/f.txt"
"$E_P2VI" -r "$A/x.diff" > "$A/old.sh"

# the user changes a second, disjoint line during the handover
run_A "$A" '%s/^L2$/L2c/:q!' '-E old.sh > new.sh 2> nerr'

if diff -q "$A/f.txt" "$A/base.txt" >/dev/null 2>&1; then
	ok "-E leaves the replayed files on disk alone"
else
	fail "-E leaves the replayed files on disk alone"
fi

printf 'L1\nL2c\nL3x\n' > "$A/want.txt"
cp "$A/base.txt" "$A/f.txt"
( cd "$A" && VI="$VI" sh new.sh ) >/dev/null 2>&1
if diff -q "$A/f.txt" "$A/want.txt" >/dev/null 2>&1; then
	ok "-E script carries the old effect plus the new edit"
else
	fail "-E script carries the old effect plus the new edit"
	tr -d '\r' < "$A/nerr" | sed 's/^/    /'
fi

# an untouched handover is a fixed point: the new script does what the old
# one did, no more
cp "$A/base.txt" "$A/f.txt"
run_A "$A" 'q!' '-E old.sh > same.sh 2> nerr2'
printf 'L1\nL2\nL3x\n' > "$A/want2.txt"
( cd "$A" && VI="$VI" sh same.sh ) >/dev/null 2>&1
if diff -q "$A/f.txt" "$A/want2.txt" >/dev/null 2>&1; then
	ok "-E without edits reproduces the script's own effect"
else
	fail "-E without edits reproduces the script's own effect"
	tr -d '\r' < "$A/nerr2" | sed 's/^/    /'
fi

# a file named after the script is opened on top of the replay's own buffers
# and joins the same output script
cp "$A/base.txt" "$A/f.txt"
printf 'g1\ng2\n' > "$A/g.txt"
cp "$A/g.txt" "$A/g.base"
run_A "$A" '%s/^g1$/G1/:q!' '-E old.sh g.txt > new2.sh 2> nerr3'
printf 'G1\ng2\n' > "$A/wantg.txt"
( cd "$A" && VI="$VI" sh new2.sh ) >/dev/null 2>&1
if diff -q "$A/f.txt" "$A/want2.txt" >/dev/null 2>&1 &&
   diff -q "$A/g.txt" "$A/wantg.txt" >/dev/null 2>&1; then
	ok "-E opens the files named after the script and emits them too"
else
	fail "-E opens the files named after the script and emits them too"
	tr -d '\r' < "$A/nerr3" | sed 's/^/    /'
fi

# the input must be a generated script, not a patch
if "$E_P2VI" -E "$A/x.diff" > /dev/null 2>&1; then
	fail "-E rejects an input that is not a patch2vi script"
else
	ok "-E rejects an input that is not a patch2vi script"
fi

# -o names the output file for any mode: here a plain diff conversion
"$E_P2VI" -r -o "$A/o1.sh" "$A/x.diff" > "$A/o1.out" 2>&1
if [ -s "$A/o1.sh" ] && [ ! -s "$A/o1.out" ] &&
   head -n1 "$A/o1.sh" | grep -q '^#!/bin/sh'; then
	ok "-o writes the script to the named file, not stdout"
else
	fail "-o writes the script to the named file, not stdout"
fi

# what -o writes is a script: a file it creates comes out executable, a file
# it replaces keeps the mode it had
rm -f "$A/o2.sh"
"$E_P2VI" -r -o "$A/o2.sh" "$A/x.diff"
printf 'placeholder\n' > "$A/o3.sh"
chmod 600 "$A/o3.sh"
"$E_P2VI" -r -o "$A/o3.sh" "$A/x.diff"
if [ -x "$A/o2.sh" ] && [ ! -x "$A/o3.sh" ]; then
	ok "-o makes a new script executable, keeps an existing file's mode"
else
	fail "-o makes a new script executable, keeps an existing file's mode"
fi

# -o may name the very script -E is updating: everything is read before
# anything is written, and the file is replaced atomically at the end
cp "$A/base.txt" "$A/f.txt"
cp "$A/old.sh" "$A/self.sh"
run_A "$A" '%s/^L2$/L2c/:q!' '-o self.sh -E self.sh 2> nerr4'
( cd "$A" && VI="$VI" sh self.sh ) >/dev/null 2>&1
if diff -q "$A/f.txt" "$A/want.txt" >/dev/null 2>&1; then
	ok "-E -o on its own script updates it in place"
else
	fail "-E -o on its own script updates it in place"
	tr -d '\r' < "$A/nerr4" | sed 's/^/    /'
fi

# -oE says the same thing in one word: clustered with -E, -o takes no file
# name of its own and reuses the script -E names
cp "$A/base.txt" "$A/f.txt"
cp "$A/old.sh" "$A/self2.sh"
run_A "$A" '%s/^L2$/L2c/:q!' '-oE self2.sh 2> nerr5'
( cd "$A" && VI="$VI" sh self2.sh ) >/dev/null 2>&1
if diff -q "$A/f.txt" "$A/want.txt" >/dev/null 2>&1; then
	ok "-oE updates the script it names in place"
else
	fail "-oE updates the script it names in place"
	tr -d '\r' < "$A/nerr5" | sed 's/^/    /'
fi

# -o keeps naming a file whenever what follows is not an -E cluster
( cd "$A" && "$E_P2VI" -r -oE2.sh x.diff ) > "$A/o4.out" 2>&1
if [ -s "$A/E2.sh" ] && [ ! -s "$A/o4.out" ]; then
	ok "-oFILE still names a file when FILE looks like a cluster"
else
	fail "-oFILE still names a file when FILE looks like a cluster"
fi
rm -f "$A/E2.sh"

# a run that fails leaves the -o file alone and drops its temp
cp "$A/old.sh" "$A/keep.sh"
cp "$A/keep.sh" "$A/keep.want"
printf 'nothing like the script\n' > "$A/drift.txt"
"$E_P2VI" -o "$A/keep.sh" -E "$A/x.diff" >/dev/null 2>&1 || true
if diff -q "$A/keep.sh" "$A/keep.want" >/dev/null 2>&1 &&
   [ ! -e "$A/keep.sh.p2v.tmp" ]; then
	ok "-o leaves the target untouched when the run fails"
else
	fail "-o leaves the target untouched when the run fails"
fi

else
	echo "  SKIP: script(1) not available"
fi

echo ""
echo "=== -e multi-block tests ==="

# Two blocks in one script: the shell spawns a $VI per block, so block 2
# starts with empty registers. Block 2 gates its edit on a search served
# from register 98, which only block 1 filled - leaking that state (or
# block 1's buffer list, which would make b0 the wrong file) rewrites
# f1.txt instead and the comparison against the shell run fails.
mkdir -p "$TMPDIR/mb/sh" "$TMPDIR/mb/e"
for d in "$TMPDIR/mb/sh" "$TMPDIR/mb/e"; do
	printf 'alpha\nbeta\n' > "$d/f1.txt"
	printf 'gamma\ndelta\n' > "$d/f2.txt"
done
cat > "$TMPDIR/mb/two.sh" <<'EOS'
#!/bin/sh -e
VI=${VI:-vi}
SEP="$(printf '\001')"
( : > /tmp/p2vi.$$ ) 2>/dev/null && P2VIF=/tmp/p2vi.$$ || P2VIF=./p2vi.$$
trap 'rm -f "$P2VIF"' EXIT
printf '%s\n' "b0:%ya 98:1s/alpha/ALPHA/:w:2q" > "$P2VIF"
EXINIT='%ya 97:? %@97' $VI -e 'f1.txt' "$P2VIF"
printf '%s\n' "b0:fr 98:%f> beta:??1s/[ag][lm][pa][hm][aa]/LEAK/:w:2q" > "$P2VIF"
EXINIT='%ya 97:? %@97' $VI -e 'f2.txt' "$P2VIF"
exit 0
EOS
chmod +x "$TMPDIR/mb/two.sh"
mb_sh_rc=0 mb_e_rc=0
mb_p2vi="$PWD/patch2vi"
( cd "$TMPDIR/mb/sh" && VI="$VI" ../two.sh ) >/dev/null 2>&1 || mb_sh_rc=$?
( cd "$TMPDIR/mb/e" && "$mb_p2vi" -e ../two.sh ) >/dev/null 2>&1 || mb_e_rc=$?
if [ "$mb_sh_rc" = 0 ] && [ "$mb_e_rc" = 0 ] &&
   diff -r "$TMPDIR/mb/sh" "$TMPDIR/mb/e" >/dev/null 2>&1 &&
   grep -q ALPHA "$TMPDIR/mb/e/f1.txt" && grep -q gamma "$TMPDIR/mb/e/f2.txt"; then
	ok "-e runs every block with its own editor state"
else
	fail "-e runs every block with its own editor state (status $mb_e_rc vs $mb_sh_rc)"
	diff -r "$TMPDIR/mb/sh" "$TMPDIR/mb/e" | sed 's/^/    /'
fi

# Several scripts in one -e: run in the order given, each in its own editor
# lifetime, and the first failure stopping the rest - what "./a.sh && ./b.sh"
# does, minus the shell and a process per script. The second script edits a
# file the first never names, so a run that stopped early leaves it alone.
mkdir -p "$TMPDIR/ms"
ms_write() {	# ms_write <script> <file> <ex body>
	cat > "$TMPDIR/ms/$1" <<EOS
#!/bin/sh -e
VI=\${VI:-vi}
( : > /tmp/p2vi.\$\$ ) 2>/dev/null && P2VIF=/tmp/p2vi.\$\$ || P2VIF=./p2vi.\$\$
trap 'rm -f "\$P2VIF"' EXIT
printf '%s\n' "$3" > "\$P2VIF"
EXINIT='%ya 97:? %@97' \$VI -e '$2' "\$P2VIF"
exit 0
EOS
	chmod +x "$TMPDIR/ms/$1"
}
ms_fresh() {
	printf 'alpha\n' > "$TMPDIR/ms/f1.txt"
	printf 'gamma\n' > "$TMPDIR/ms/f2.txt"
}
ms_write one.sh f1.txt "b0:1s/alpha/ALPHA/:w:2q"
ms_write two.sh f2.txt "b0:1s/gamma/GAMMA/:w:2q"
ms_write bad.sh f1.txt "b0:2q!1"	# quits with the status a failed hunk does
ms_p2vi="$PWD/patch2vi"

ms_fresh
ms_rc=0
( cd "$TMPDIR/ms" && "$ms_p2vi" -e one.sh two.sh ) >/dev/null 2>&1 || ms_rc=$?
if [ "$ms_rc" = 0 ] && grep -q ALPHA "$TMPDIR/ms/f1.txt" &&
   grep -q GAMMA "$TMPDIR/ms/f2.txt"; then
	ok "-e runs every script it is given, in order"
else
	fail "-e runs every script it is given, in order (status $ms_rc)"
fi

ms_fresh
ms_rc=0
( cd "$TMPDIR/ms" && "$ms_p2vi" -e bad.sh two.sh ) >/dev/null 2>&1 || ms_rc=$?
if [ "$ms_rc" != 0 ] && grep -q gamma "$TMPDIR/ms/f2.txt"; then
	ok "-e stops at the first script that fails"
else
	fail "-e stops at the first script that fails (status $ms_rc)"
fi

echo ""
echo "=== replay (-C) tests ==="

# The compat session replays a generated script in ONE editor: buffers
# persist across blocks, nothing is written, and the last block hands the
# session over. The handover is driven by P2VI_EX, which writes the
# session's buffers out so their in-RAM state can be checked - the disk
# copies of the replayed files must stay untouched.
if command -v script >/dev/null 2>&1; then

R="$TMPDIR/rp"
R_P2VI="$PWD/patch2vi"
mkdir -p "$R"

# <script> <P2VI_EX>: replay under a pty, ignoring the exit status (the
# derivation stage after the handover is not implemented yet)
run_pr() {
	pty "P2VI_EX=$2" \
		"sh -c 'cd $R && $R_P2VI -C $1 /dev/null'" \
		> "$R/log" 2>&1
}

printf 'a\nb\nc\n' > "$R/f1.txt"
printf 'x\ny\nz\n' > "$R/f2.txt"
cp "$R/f1.txt" "$R/f1.orig"
cp "$R/f2.txt" "$R/f2.orig"

# Two blocks over the same files, listed in a DIFFERENT order by each:
# block 2 edits its own b1 (f1.txt), which is the session's buffer 0, and
# its edit only matches if it sees block 1's edit rather than the disk.
{
	printf '#!/bin/sh -e\nVI=${VI:-vi}\n'
	printf 'SEP="$(printf '"'"'\\001'"'"')"\n'
	printf 'ESC="$(printf '"'"'\\002'"'"')"\n'
	printf 'printf '"'"'%%s\\n'"'"' "|sc! ${ESC}${SEP}|:vis 3${SEP}b0${SEP}%%ya 98${SEP}1s/a/A/${SEP}vis 2${SEP}b0${SEP}w${SEP}b1${SEP}w${SEP}2q" > "$P2VIF"\n'
	printf "EXINIT='%%ya 97:? %%@97' \$VI -e 'f1.txt' 'f2.txt' \"\$P2VIF\"\n"
	printf 'printf '"'"'%%s\\n'"'"' "|sc! ${ESC}${SEP}|:vis 3${SEP}b1${SEP}1s/A/AA/${SEP}vis 2${SEP}b1${SEP}w${SEP}2q" > "$P2VIF"\n'
	printf "EXINIT='%%ya 97:? %%@97' \$VI -e 'f2.txt' 'f1.txt' \"\$P2VIF\"\n"
	printf 'exit 0\n'
} > "$R/two.sh"

rm -f "$R/out0" "$R/out1"
run_pr two.sh "b0:w! $R/out0:b1:w! $R/out1:q!"
if [ "$(cat "$R/out0" 2>/dev/null)" = "$(printf 'AA\nb\nc')" ]; then
	ok "replay: a block sees the previous block's edits (b<N> remapped)"
else
	fail "replay: a block sees the previous block's edits (b<N> remapped)"
	cat "$R/out0" 2>/dev/null | sed 's/^/    /'
fi
if diff -q "$R/f1.txt" "$R/f1.orig" >/dev/null 2>&1 &&
   diff -q "$R/f2.txt" "$R/f2.orig" >/dev/null 2>&1; then
	ok "replay writes nothing to disk"
else
	fail "replay writes nothing to disk"
fi

# A real generated script (multi-file, multi-line body) replays the same
# way: its edits land in the buffers, never in the files
printf 'a\nB\nc\n' > "$R/f1.want"
printf 'x\nY\nz\n' > "$R/f2.want"
{
	diff -u "$R/f1.orig" "$R/f1.want" | sed '1s|.*|--- a/f1.txt|;2s|.*|+++ b/f1.txt|'
	diff -u "$R/f2.orig" "$R/f2.want" | sed '1s|.*|--- a/f2.txt|;2s|.*|+++ b/f2.txt|'
} > "$R/d.diff" || true
"$R_P2VI" -r "$R/d.diff" > "$R/real.sh"
sed -i "s|\$VI -e '[^']*' '[^']*'|\$VI -e 'f1.txt' 'f2.txt'|" "$R/real.sh"
rm -f "$R/out0" "$R/out1"
run_pr real.sh "b0:w! $R/out0:b1:w! $R/out1:q!"
if diff -q "$R/out0" "$R/f1.want" >/dev/null 2>&1 &&
   diff -q "$R/out1" "$R/f2.want" >/dev/null 2>&1 &&
   diff -q "$R/f1.txt" "$R/f1.orig" >/dev/null 2>&1; then
	ok "replay applies a generated script in RAM only"
else
	fail "replay applies a generated script in RAM only"
fi

# A script that does not apply must abort the replay, not hand over a
# half-patched session: phase 1/2 failures are made fatal and loud
printf 'zzz\nqqq\n' > "$R/f1.txt"
run_pr real.sh 'q!'
cp "$R/f1.orig" "$R/f1.txt"
if tr -d '\r' < "$R/log" | grep -q "^replay: block 1 failed with status 1"; then
	ok "replay aborts when a replayed block fails"
else
	fail "replay aborts when a replayed block fails"
	tail -3 "$R/log" | sed 's/^/    /'
fi

# Compat derivation (stage B2). -C replays the origin and target, hands the tree to the
# user, and turns the user's merge into a gated compat block emitted after the
# target's own hunk. The gate's probe comes from where the origin actually
# landed (its inserted line), so on an origin tree the block fires and merges,
# while on a clean tree the gate self-skips before any edit and the target
# applies alone.
coderive() {	# <origin.sh> <target.sh> <P2VI_EX>: emit -C result to $R/new.sh
	rm -f "$R/new.sh"
	pty "P2VI_EX=$3" \
		"sh -c 'cd $R && $R_P2VI -C $1 $2 > $R/new.sh 2>$R/nerr'" > /dev/null 2>&1
}

# stack <script...>: run the chain from the first, the applied-set form. Each
# script appends its basename to $P2VI_PATCH and invokes the next, so later
# scripts' compat blocks see exactly the origins that ran. The chain form is
# what replaced the old "o.sh && t.sh" application: scripts must be
# executable for the "$next" hop inside the tail.
stack() {
	( cd "$R" && chmod +x "$@" && VI="$VI" sh "$@" ) >/dev/null 2>&1
}

# The compat block and the target's own hunk are independent units that stack:
# patch2vi reasons across neither. origin (x1) inserts PROBE, which gives the
# gate a unique landmark; the target (x2) makes its own change (L3 -> L3x); and
# in the handover the user edits a third, disjoint line (L2 -> L2c) - the compat
# edit. On an origin tree all three compound; on a clean tree only x2 runs.
printf 'L1\nL2\nL3\n' > "$R/draw.orig"
printf -- '--- a/draw.c\n+++ b/draw.c\n@@ -1,3 +1,4 @@\n L1\n+PROBE\n L2\n L3\n' > "$R/x1.diff"
printf -- '--- a/draw.c\n+++ b/draw.c\n@@ -1,3 +1,3 @@\n L1\n L2\n-L3\n+L3x\n' > "$R/x2.diff"
"$R_P2VI" -r "$R/x1.diff" > "$R/x1.sh"
"$R_P2VI" -r "$R/x2.diff" > "$R/x2.sh"
cp "$R/draw.orig" "$R/draw.c"	# pre-origin tree the replay reads
coderive x1.sh x2.sh '%s/^L2$/L2c/:q!'

if grep -q '^# Compat [0-9]* src=x1.sh' "$R/new.sh" 2>/dev/null; then
	ok "compat: -C emits a gated post-block from the user's merge"
else
	fail "compat: -C emits a gated post-block from the user's merge"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi

# origin tree: x1 lands PROBE, the compat block fires (L2 -> L2c) because PROBE
# is present, and x2 applies its own change (L3 -> L3x) - the units stack
cp "$R/draw.orig" "$R/draw.c"
stack x1.sh new.sh
if [ "$(cat "$R/draw.c")" = "$(printf 'L1\nPROBE\nL2c\nL3x')" ]; then
	ok "compat: fires on an origin tree, stacks with the target hunk"
else
	fail "compat: fires on an origin tree, stacks with the target hunk"
	sed 's/^/    /' "$R/draw.c"
fi

# clean tree: PROBE is absent, so the gate quits before any edit; only x2 runs
cp "$R/draw.orig" "$R/draw.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
if [ "$(cat "$R/draw.c")" = "$(printf 'L1\nL2\nL3x')" ]; then
	ok "compat: gate no-ops on a clean tree, target applies alone"
else
	fail "compat: gate no-ops on a clean tree, target applies alone"
	sed 's/^/    /' "$R/draw.c"
fi

# A collision the target cannot survive: the origin deletes the very line the
# target rewrites, so on the origin tree the target's hunk has nothing to find
# and misses. That is the case a compat block exists for, and the tree the fix
# is written against is the one the miss leaves - so the replay reports the miss
# and carries on (QF2), where a plain run of the same script quits at it.
# Without that the derivation would measure a tree the target barely touched,
# and rebuild - which regenerates a base with no block to relax its quit chain -
# could never re-derive the block it just took off.
i=1
: > "$R/kk.orig"
while [ $i -le 12 ]; do printf 'K%d\n' "$i" >> "$R/kk.orig"; i=$((i + 1)); done
grep -v '^K3$' "$R/kk.orig" > "$R/kk.a"		# origin: K3 is gone
sed 's/^K3$/K3b/' "$R/kk.orig" > "$R/kk.b"	# target: K3 -> K3b, the same line
for v in a b; do
	diff -u "$R/kk.orig" "$R/kk.$v" |
		sed -e '1s|.*|--- a/kk.c|' -e '2s|.*|+++ b/kk.c|' > "$R/k$v.diff"
	"$R_P2VI" -r "$R/k$v.diff" > "$R/k$v.sh"
done
cp "$R/kk.orig" "$R/kk.c"
coderive ka.sh kb.sh '%s/^K4$/K4x/:q!'	# what the author does about the miss
cp "$R/kk.orig" "$R/kk.c"
stack ka.sh new.sh
k_both="$(tr '\n' ' ' < "$R/kk.c")"
cp "$R/kk.orig" "$R/kk.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
k_clean="$(tr '\n' ' ' < "$R/kk.c")"
case $k_both in *"K2 K4x "*) k1=1 ;; *) k1=0 ;; esac
case $k_clean in *"K3b K4 "*) k2=1 ;; *) k2=0 ;; esac
if [ -s "$R/new.sh" ] && [ "$k1" = 1 ] && [ "$k2" = 1 ]; then
	ok "compat: a target hunk the origin makes miss still derives its block"
else
	fail "compat: a target hunk the origin makes miss still derives its block"
	echo "    origin=[$k_both]"
	echo "    clean=[$k_clean]"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /' | head -3
fi

# An origin whose inserted line is not unique needed a discriminating probe
# under the gate model; names always discriminate (for a name there is no
# "two trees apart" failure - either the origin is in the applied set or it
# is not), so the hard error is gone and this refusal is accepted as lost.

# -C replays the origin AND the target into one session before the handover,
# so the target must apply cleanly on the origin-modified tree. That exercises
# the block boundary: the origin leaves its cursor deep in the buffer, and a
# leftover row would steer the target's searches - so every buffer is rewound to
# the top before the next block. origin (x1) inserts PROBE (the gate landmark);
# target (x2) changes a disjoint line (L3 -> L3x); the compat block is emitted
# AFTER the target and gated on PROBE. In the handover the user edits L2 -> L2c.
printf 'L1\nL2\nL3\n' > "$R/po.orig"
printf -- '--- a/po.c\n+++ b/po.c\n@@ -1,3 +1,4 @@\n L1\n+PROBE\n L2\n L3\n' > "$R/p1.diff"
printf -- '--- a/po.c\n+++ b/po.c\n@@ -1,3 +1,3 @@\n L1\n L2\n-L3\n+L3x\n' > "$R/p2.diff"
"$R_P2VI" -r "$R/p1.diff" > "$R/p1.sh"
"$R_P2VI" -r "$R/p2.diff" > "$R/p2.sh"
cp "$R/po.orig" "$R/po.c"	# pre-origin tree the replay reads
coderive p1.sh p2.sh '%s/^L2$/L2c/:q!'
if grep -q '^# Compat [0-9]* src=p1.sh' "$R/new.sh" 2>/dev/null; then
	ok "compat: -C emits a gated post-block from the user's merge"
else
	fail "compat: -C emits a gated post-block from the user's merge"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi

# origin tree: PROBE present, so the target's own change lands (L3 -> L3x) and
# then the postfix compat block fires (L2 -> L2c) - the units stack
cp "$R/po.orig" "$R/po.c"
stack p1.sh new.sh
if [ "$(cat "$R/po.c")" = "$(printf 'L1\nPROBE\nL2c\nL3x')" ]; then
	ok "compat: -C fires on an origin tree, stacks with the target hunk"
else
	fail "compat: -C fires on an origin tree, stacks with the target hunk"
	sed 's/^/    /' "$R/po.c"
fi

# clean tree: PROBE absent, so the postfix gate quits before its edit; only the
# target's own change lands
cp "$R/po.orig" "$R/po.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
if [ "$(cat "$R/po.c")" = "$(printf 'L1\nL2\nL3x')" ]; then
	ok "compat: -C postfix gate no-ops on a clean tree"
else
	fail "compat: -C postfix gate no-ops on a clean tree"
	sed 's/^/    /' "$R/po.c"
fi

# A second positional is an already written compat patch, applied to the
# post-origin+target tree before the handover: the author does not retype a
# resolution they already have, and the editor still opens on top of it, so the
# derived block covers the pre-applied edits AND whatever the user adds.
coderive3() {	# <origin.sh> <target.sh> <compat> <P2VI_EX>: out $R/new.sh
	rm -f "$R/new.sh"
	pty "P2VI_EX=$4" \
		"sh -c 'cd $R && $R_P2VI -C $1 $2 $3 > $R/new.sh 2>$R/nerr'" \
		> /dev/null 2>&1
}

# origin (b1) inserts PROBE, target (b2) changes A6 -> A6x, and the compat patch
# (written against the tree both leave behind) changes A3 -> A3c. The user quits
# without touching anything, so the derived block is exactly that patch.
printf 'A1\nA2\nA3\nA4\nA5\nA6\n' > "$R/pb.orig"
printf -- '--- a/pb.c\n+++ b/pb.c\n@@ -1,3 +1,4 @@\n A1\n+PROBE\n A2\n A3\n' > "$R/b1.diff"
printf -- '--- a/pb.c\n+++ b/pb.c\n@@ -4,3 +4,3 @@\n A4\n A5\n-A6\n+A6x\n' > "$R/b2.diff"
printf -- '--- a/pb.c\n+++ b/pb.c\n@@ -3,3 +3,3 @@\n A2\n-A3\n+A3c\n A4\n' > "$R/bc.diff"
"$R_P2VI" -r "$R/b1.diff" > "$R/b1.sh"
"$R_P2VI" -r "$R/b2.diff" > "$R/b2.sh"
cp "$R/pb.orig" "$R/pb.c"	# pre-origin tree the replay reads
coderive3 b1.sh b2.sh bc.diff ':q!'
if grep -q '^# Compat [0-9]* src=b1.sh' "$R/new.sh" 2>/dev/null; then
	ok "compat: -C derives a block from a pre-applied diff alone"
else
	fail "compat: -C derives a block from a pre-applied diff alone"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi

cp "$R/pb.orig" "$R/pb.c"
stack b1.sh new.sh
if [ "$(cat "$R/pb.c")" = "$(printf 'A1\nPROBE\nA2\nA3c\nA4\nA5\nA6x')" ]; then
	ok "compat: pre-applied diff fires gated on an origin tree"
else
	fail "compat: pre-applied diff fires gated on an origin tree"
	sed 's/^/    /' "$R/pb.c"
fi

cp "$R/pb.orig" "$R/pb.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
if [ "$(cat "$R/pb.c")" = "$(printf 'A1\nA2\nA3\nA4\nA5\nA6x')" ]; then
	ok "compat: pre-applied diff still self-skips on a clean tree"
else
	fail "compat: pre-applied diff still self-skips on a clean tree"
	sed 's/^/    /' "$R/pb.c"
fi

# -m must hold across replay blocks: the body run clears bit 4 (the body's
# own chatter is not the session's to silence), and it used to stay cleared,
# so deriving on top of a target that carries compat blocks printed its
# *p2vi-sec-N* scaffold loads from every block after the first
cp "$R/pb.orig" "$R/pb.c"
p2v_out=$(pty "P2VI_EX=q" \
	"sh -c 'cd $R && $R_P2VI -C b1.sh new.sh "" > /dev/null 2>&1'")
printf '%s' "$p2v_out" | grep -q 'p2vi-sec' &&
	ok "compat: a target's scaffold loads announce themselves" ||
	fail "compat: a target's scaffold loads announce themselves"
cp "$R/pb.orig" "$R/pb.c"
p2v_out=$(pty "P2VI_EX=q" \
	"sh -c 'cd $R && $R_P2VI -C b1.sh new.sh "" -em > /dev/null 2>&1'")
if printf '%s' "$p2v_out" | grep -q 'p2vi-sec'; then
	fail "compat: -m holds across replay blocks"
	printf '%s\n' "$p2v_out" | sed 's/^/    /'
else
	ok "compat: -m holds across replay blocks"
fi

# '' skips the fix slot without giving up the positionals behind it: the
# trailing nextvi command line reaches the handover as with -E. Its files open
# only after the baseline snapshot, so they are visible in the session but
# never part of the derived diff.
coderive2h() {	# <origin.sh> <target.sh> <P2VI_EX>: out $R/new.sh
	rm -f "$R/new.sh"
	pty "P2VI_EX=$3" \
		"sh -c 'cd $R && $R_P2VI -C $1 $2 '' extra.c > $R/new.sh 2>$R/nerr'" \
		> /dev/null 2>&1
}
printf 'X1\n' > "$R/extra.c"
cp "$R/pb.orig" "$R/pb.c"
# buffer 0 is the replayed file; the handover opened extra.c last, so land on
# b0 before editing it - and leave extra.c untouched
coderive2h b1.sh b2.sh ':b0:%s/^A3$/A3z/:q!'
if grep -q '^# Compat [0-9]* src=b1.sh' "$R/new.sh" 2>/dev/null &&
	! grep -q 'extra\.c' "$R/new.sh"; then
	ok "compat: '' skips the fix slot; hand files stay out of the diff"
else
	fail "compat: '' skips the fix slot; hand files stay out of the diff"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi

cp "$R/pb.orig" "$R/pb.c"
printf 'X1\n' > "$R/extra.c"
# an edit made only in a hand file derives nothing: it is not ours to ship
coderive2h b1.sh b2.sh '%s/^X1$/X1e/:q!'
if [ ! -s "$R/new.sh" ] && grep -q 'no compat patch derived' "$R/nerr"; then
	ok "compat: edits to hand files alone derive nothing"
else
	fail "compat: edits to hand files alone derive nothing"
	head -3 "$R/new.sh" 2>/dev/null | sed 's/^/    /'
fi

# the same fix in script form is replayed as one more block of the session, and
# the handover is not subverted: the user's own edit (A5 -> A5u) compounds with
# the pre-applied A3 -> A3c in a single derived block
"$R_P2VI" -r "$R/bc.diff" > "$R/bc.sh"
cp "$R/pb.orig" "$R/pb.c"
coderive3 b1.sh b2.sh bc.sh '%s/^A5$/A5u/:q!'
cp "$R/pb.orig" "$R/pb.c"
stack b1.sh new.sh
if [ "$(cat "$R/pb.c")" = "$(printf 'A1\nPROBE\nA2\nA3c\nA4\nA5u\nA6x')" ]; then
	ok "compat: pre-applied script plus the user's own edit in one block"
else
	fail "compat: pre-applied script plus the user's own edit in one block"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
	sed 's/^/    /' "$R/pb.c"
fi

# a compat patch whose pre-image is not on the tree is a hard error, not a
# silent skip: the author would ship a block they never wrote
printf -- '--- a/pb.c\n+++ b/pb.c\n@@ -3,3 +3,3 @@\n A2\n-NOPE\n+A3c\n A4\n' > "$R/bb.diff"
cp "$R/pb.orig" "$R/pb.c"
coderive3 b1.sh b2.sh bb.diff ':q!'
if [ ! -s "$R/new.sh" ] && tr -d '\r' < "$R/nerr" | grep -q 'does not apply'; then
	ok "compat: a pre-applied diff that does not apply is refused"
else
	fail "compat: a pre-applied diff that does not apply is refused"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi

# The origin's landing is what a gate probes, and the target can overwrite it:
# origin (c1) changes X3 -> X3o and target (c2) then replaces the whole X2..X4
# region, so the post-origin+target text carries no trace of the origin - it is
# byte-identical to a target-only tree. The probe must come from the
# post-origin, pre-target text, which is what a sensor reads at run time (every
# sensor runs before any body writes).
printf 'X1\nX2\nX3\nX4\nX5\nX6\n' > "$R/pc.orig"
printf -- '--- a/pc.c\n+++ b/pc.c\n@@ -2,3 +2,3 @@\n X2\n-X3\n+X3o\n X4\n' > "$R/c1.diff"
printf -- '--- a/pc.c\n+++ b/pc.c\n@@ -1,5 +1,5 @@\n X1\n-X2\n-X3\n-X4\n+T2\n+T3\n+T4\n X5\n' > "$R/c2.diff"
"$R_P2VI" -r "$R/c1.diff" > "$R/c1.sh"
"$R_P2VI" -r "$R/c2.diff" > "$R/c2.sh"
cp "$R/pc.orig" "$R/pc.c"
coderive c1.sh c2.sh '%s/^X6$/X6c/:q!'
if grep -q '^# Compat [0-9]* src=c1.sh' "$R/new.sh" 2>/dev/null; then
	ok "compat: a gate derives though the target overwrote the origin's line"
else
	fail "compat: a gate derives though the target overwrote the origin's line"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi

cp "$R/pc.orig" "$R/pc.c"
stack c1.sh new.sh
o1=$(cat "$R/pc.c")
cp "$R/pc.orig" "$R/pc.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
o2=$(cat "$R/pc.c")
if [ "$o1" = "$(printf 'X1\nT2\nT3\nT4\nX5\nX6c')" ] &&
   [ "$o2" = "$(printf 'X1\nT2\nT3\nT4\nX5\nX6')" ]; then
	ok "compat: overwritten-trace gate fires on origin, no-ops on clean"
else
	fail "compat: overwritten-trace gate fires on origin, no-ops on clean"
	printf '%s\n--\n%s\n' "$o1" "$o2" | sed 's/^/    /'
fi

# A block whose file the origin never touched still fires when its origin is
# in the applied set: "did this origin land" is a property of the tree, so the
# block over pe.c is decided by the presence of d1.sh in the set, not by any
# trace in the file it edits.
printf 'Y1\nY2\nY3\n' > "$R/pd.orig"
printf 'Z1\nZ2\nZ3\n' > "$R/pe.orig"
printf -- '--- a/pd.c\n+++ b/pd.c\n@@ -1,3 +1,4 @@\n Y1\n+YPROBE\n Y2\n Y3\n' > "$R/d1.diff"
printf -- '--- a/pe.c\n+++ b/pe.c\n@@ -1,3 +1,3 @@\n Z1\n-Z2\n+Z2t\n Z3\n' > "$R/d2.diff"
"$R_P2VI" -r "$R/d1.diff" > "$R/d1.sh"
"$R_P2VI" -r "$R/d2.diff" > "$R/d2.sh"
cp "$R/pd.orig" "$R/pd.c"
cp "$R/pe.orig" "$R/pe.c"
coderive d1.sh d2.sh '%s/^Z3$/Z3c/:q!'
if grep -q '^# Compat [0-9]* src=d1.sh' "$R/new.sh" 2>/dev/null; then
	ok "compat: a block over an untouched file fires from the applied set"
else
	fail "compat: a block over an untouched file fires from the applied set"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi

cp "$R/pd.orig" "$R/pd.c"
cp "$R/pe.orig" "$R/pe.c"
stack d1.sh new.sh
o1=$(cat "$R/pe.c")
cp "$R/pd.orig" "$R/pd.c"
cp "$R/pe.orig" "$R/pe.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
o2=$(cat "$R/pe.c")
if [ "$o1" = "$(printf 'Z1\nZ2t\nZ3c')" ] &&
   [ "$o2" = "$(printf 'Z1\nZ2t\nZ3')" ]; then
	ok "compat: block over an untouched file fires on origin, no-ops on clean"
	cp "$R/new.sh" "$R/xf.sh"
else
	fail "compat: block over an untouched file fires on origin, no-ops on clean"
	printf '%s\n--\n%s\n' "$o1" "$o2" | sed 's/^/    /'
fi

# A compat patch is one unified diff, however many files it spans: one
# invocation reshaping two buffers must yield ONE block - one metadata region,
# one gate, one staged body - whose === COMPAT PATCH === carries both files.
printf 'H1\nH2\nH3\n' > "$R/ph.orig"
printf 'I1\nI2\nI3\n' > "$R/pi.orig"
printf -- '--- a/ph.c\n+++ b/ph.c\n@@ -1,3 +1,4 @@\n H1\n+HPROBE\n H2\n H3\n' > "$R/g1.diff"
printf -- '--- a/ph.c\n+++ b/ph.c\n@@ -1,3 +1,3 @@\n H1\n-H2\n+H2t\n H3\n--- a/pi.c\n+++ b/pi.c\n@@ -1,3 +1,3 @@\n I1\n-I2\n+I2t\n I3\n' > "$R/g2.diff"
"$R_P2VI" -r "$R/g1.diff" > "$R/g1.sh"
"$R_P2VI" -r "$R/g2.diff" > "$R/g2.sh"
cp "$R/ph.orig" "$R/ph.c"
cp "$R/pi.orig" "$R/pi.c"
coderive g1.sh g2.sh ':e ph.c:%s/^H3$/H3c/:e pi.c:%s/^I3$/I3c/:q!'
nreg="$(grep -c '^=== PATCH2VI COMPAT [0-9]* src=' "$R/new.sh" 2>/dev/null || true)"
ngate="$(grep -c '^=== GATE ' "$R/new.sh" 2>/dev/null || true)"
nbody="$(grep -c '^# Compat [0-9]* src=g1.sh' "$R/new.sh" 2>/dev/null || true)"
nfile="$(sed -n '/^=== COMPAT PATCH ===$/,/^=== END /p' "$R/new.sh" |
	 grep -c '^--- ' || true)"
if [ "$nreg" = 1 ] && [ "$ngate" = 0 ] && [ "$nbody" = 1 ] && [ "$nfile" = 2 ]; then
	ok "compat: a two-file compat patch is one block, one diff"
else
	fail "compat: a two-file compat patch is one block, one diff"
	echo "    regions=$nreg gates=$ngate bodies=$nbody files=$nfile"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /' | head -3
fi

cp "$R/ph.orig" "$R/ph.c"
cp "$R/pi.orig" "$R/pi.c"
stack g1.sh new.sh
o1="$(cat "$R/ph.c")|$(cat "$R/pi.c")"
cp "$R/ph.orig" "$R/ph.c"
cp "$R/pi.orig" "$R/pi.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
o2="$(cat "$R/ph.c")|$(cat "$R/pi.c")"
if [ "$o1" = "$(printf 'H1\nHPROBE\nH2t\nH3c|I1\nI2t\nI3c')" ] &&
   [ "$o2" = "$(printf 'H1\nH2t\nH3|I1\nI2t\nI3')" ]; then
	ok "compat: one block edits every file it spans, gated once"
	cp "$R/new.sh" "$R/mf.sh"
else
	fail "compat: one block edits every file it spans, gated once"
	printf '%s\n--\n%s\n' "$o1" "$o2" | sed 's/^/    /'
fi

# A multi-file compat patch is gated as a whole: it fires on every file it
# spans when its origin is in the applied set, and on none of them when it is
# not. A name cannot tell half a landing apart, so a partial application is
# indistinguishable from the whole origin (accepted loss; apply from a clean
# tree or assert P2VI_PATCH to keep the answer honest).
printf 'J1\nJ2\nJ3\n' > "$R/pj.orig"
printf 'K1\nK2\nK3\n' > "$R/pk.orig"
printf -- '--- a/pj.c\n+++ b/pj.c\n@@ -1,3 +1,4 @@\n J1\n+JPROBE\n J2\n J3\n--- a/pk.c\n+++ b/pk.c\n@@ -1,3 +1,4 @@\n K1\n+KPROBE\n K2\n K3\n' > "$R/h1.diff"
printf -- '--- a/pj.c\n+++ b/pj.c\n@@ -1,3 +1,3 @@\n J1\n-J2\n+J2t\n J3\n--- a/pk.c\n+++ b/pk.c\n@@ -1,3 +1,3 @@\n K1\n-K2\n+K2t\n K3\n' > "$R/h2.diff"
"$R_P2VI" -r "$R/h1.diff" > "$R/h1.sh"
"$R_P2VI" -r "$R/h2.diff" > "$R/h2.sh"
cp "$R/pj.orig" "$R/pj.c"
cp "$R/pk.orig" "$R/pk.c"
coderive h1.sh h2.sh ':e pj.c:%s/^J3$/J3c/:e pk.c:%s/^K3$/K3c/:q!'
if grep -q '^# Compat [0-9]* src=h1.sh' "$R/new.sh" 2>/dev/null; then
	ok "compat: a two-file block edits both files, gated as one"
else
	fail "compat: a two-file block edits both files, gated as one"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /' | head -3
fi

cp "$R/pj.orig" "$R/pj.c"
cp "$R/pk.orig" "$R/pk.c"
stack h1.sh new.sh
o1="$(cat "$R/pj.c")|$(cat "$R/pk.c")"
cp "$R/pj.orig" "$R/pj.c"
cp "$R/pk.orig" "$R/pk.c"
( cd "$R" && VI="$VI" sh new.sh ) >/dev/null 2>&1
o2="$(cat "$R/pj.c")|$(cat "$R/pk.c")"
if [ "$o1" = "$(printf 'J1\nJPROBE\nJ2t\nJ3c|K1\nKPROBE\nK2t\nK3c')" ] &&
   [ "$o2" = "$(printf 'J1\nJ2t\nJ3|K1\nK2t\nK3')" ]; then
	ok "compat: a two-file block fires on origin, no-ops on clean"
else
	fail "compat: a two-file block fires on origin, no-ops on clean"
	printf '%s\n--\n%s\n' "$o1" "$o2" | sed 's/^/    /'
fi

coderiveq() {	# coderive with QF2=1: the target is expected to miss
	rm -f "$R/new.sh"
	pty "P2VI_EX=$3" \
		"sh -c 'cd $R && QF2=1 $R_P2VI -C $1 $2 > $R/new.sh 2>$R/nerr'" \
		> /dev/null 2>&1
}

# A host group the origin made impossible must not take the rest of the body
# with it. On a tree where some origin is present the host runs best-effort
# through register 211, which reads its flag with a register search - and ex's
# register redirection is global (xfr), so leaving it set sends every later
# multi-line search to the flag instead of the file cache. Origin (e1) deletes
# W2, target (e2) edits W2 in pf.c (a guaranteed miss, so the relaxed chain
# runs) and inserts into pg.c behind a duplicated anchor, whose group searches
# the cache and must still find it.
printf 'W1\nW2\nW3\nW4\n' > "$R/pf.orig"
printf 'V1\nDUP\nV2\nDUP\nV3\n' > "$R/pg.orig"
printf -- '--- a/pf.c\n+++ b/pf.c\n@@ -1,3 +1,2 @@\n W1\n-W2\n W3\n' > "$R/e1.diff"
printf -- '--- a/pf.c\n+++ b/pf.c\n@@ -1,3 +1,3 @@\n W1\n-W2\n+W2t\n W3\n--- a/pg.c\n+++ b/pg.c\n@@ -3,3 +3,4 @@\n V2\n DUP\n+NEW\n V3\n' > "$R/e2.diff"
"$R_P2VI" -r "$R/e1.diff" > "$R/e1.sh"
"$R_P2VI" -r "$R/e2.diff" > "$R/e2.sh"
cp "$R/pf.orig" "$R/pf.c"
cp "$R/pg.orig" "$R/pg.c"
coderiveq e1.sh e2.sh ':e pg.c:%s/^V3$/V3c/:q!'
cp "$R/pf.orig" "$R/pf.c"
cp "$R/pg.orig" "$R/pg.c"
( cd "$R" && chmod +x e1.sh new.sh && VI="$VI" QF2=1 sh e1.sh new.sh ) >/dev/null 2>&1
if [ "$(cat "$R/pf.c")" = "$(printf 'W1\nW3\nW4')" ] &&
   [ "$(cat "$R/pg.c")" = "$(printf 'V1\nDUP\nV2\nDUP\nNEW\nV3c')" ]; then
	ok "compat: a host miss does not derail the groups after it"
else
	fail "compat: a host miss does not derail the groups after it"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
	sed 's/^/    /' "$R/pf.c" "$R/pg.c"
fi

# QF2=1 is the run's own decision and outranks the script's: a compat block
# rewrites register 211 at run time (its quit policy, the host override), and
# those rewrites must re-point the assert, never re-enable asserting. The block
# below misses its first group on a tree where the origin's line was taken away,
# which is a phase-2 error the last block over the file asserts on: unset it
# quits before anything is written, QF2=1 reports and finishes the rest.
printf 'Q1\nQ2\nQ3\nQ4\nQ5\n' > "$R/q.orig"
printf -- '--- a/q.c\n+++ b/q.c\n@@ -1,3 +1,4 @@\n Q1\n+QPROBE\n Q2\n Q3\n' > "$R/qo.diff"
printf -- '--- a/q.c\n+++ b/q.c\n@@ -3,3 +3,3 @@\n Q3\n Q4\n-Q5\n+Q5t\n' > "$R/qt.diff"
"$R_P2VI" -r "$R/qo.diff" > "$R/qo.sh"
"$R_P2VI" -r "$R/qt.diff" > "$R/qt.sh"
cp "$R/q.orig" "$R/q.c"
coderive qo.sh qt.sh '%s/^Q2$/Q2c/:%s/^Q4$/Q4c/:q!'
cp "$R/q.orig" "$R/q.c"
stack qo.sh					# the origin's tree
sed '/^Q2$/d' "$R/q.c" > "$R/q.worn"	# the compat block's first group misses
cp "$R/q.worn" "$R/q.c"
( cd "$R" && P2VI_PATCH="qo.sh" VI="$VI" sh new.sh ) >/dev/null 2>&1
q_strict="$(tr '\n' '|' < "$R/q.c")"
cp "$R/q.worn" "$R/q.c"
( cd "$R" && P2VI_PATCH="qo.sh" VI="$VI" QF2=1 sh new.sh ) >/dev/null 2>&1
q_soft="$(tr '\n' '|' < "$R/q.c")"
if [ "$q_strict" = 'Q1|QPROBE|Q3|Q4|Q5|' ] &&
   [ "$q_soft" = 'Q1|QPROBE|Q3|Q4c|Q5t|' ]; then
	ok "compat: QF2=1 survives a block's own quit policy"
else
	fail "compat: QF2=1 survives a block's own quit policy"
	echo "    strict=[$q_strict] qf2=[$q_soft]"
fi

# Stage B3: storage, round-trip and stacking. The -C script above (new.sh from
# draw.c) carries a pre COMPAT region after exit 0. A plain regen must
# reproduce the whole script - host block and compat region - byte-identically,
# without re-running the origin and without an editor.
dregen() {	# <script>: regenerate it to $R/dregen.sh, reading its own storage
	rm -f "$R/dregen.sh"
	( cd "$R" && "$R_P2VI" -r "$1" > "$R/dregen.sh" 2> "$R/derr" ) || true
}
# a stored block round-trips through storage: a regen reparses the recorded
# region and emits the same script
cp "$R/pd.orig" "$R/pd.c"
cp "$R/pe.orig" "$R/pe.c"
dregen xf.sh
if [ -s "$R/dregen.sh" ] && cmp -s "$R/xf.sh" "$R/dregen.sh"; then
	ok "compat: a regen round-trips a stored compat block byte-identically"
else
	fail "compat: a regen round-trips a stored compat block byte-identically"
	tr -d '\r' < "$R/derr" | sed 's/^/    /'
	diff "$R/xf.sh" "$R/dregen.sh" | head -10 | sed 's/^/    /'
fi

# The built-in differ must not overextend. A span wider than the LCS table budget
# (DIFF_MAX_CELLS) used to degrade to delete-all/insert-all, so two edits far
# apart in a big file came out as a diff rewriting everything between them.
# Patience anchoring splits such a span on its unique common lines instead, so
# the compat patch stays the handful of lines that really changed.
awk 'BEGIN{for (i = 1; i <= 2200; i++) printf "L%04d\n", i}' > "$R/big.orig"
printf -- '--- a/big.c\n+++ b/big.c\n@@ -1,3 +1,4 @@\n L0001\n+BIGPROBE\n L0002\n L0003\n' > "$R/k1.diff"
printf -- '--- a/big.c\n+++ b/big.c\n@@ -1099,3 +1099,3 @@\n L1099\n-L1100\n+L1100t\n L1101\n' > "$R/k2.diff"
"$R_P2VI" -r "$R/k1.diff" > "$R/k1.sh"
"$R_P2VI" -r "$R/k2.diff" > "$R/k2.sh"
cp "$R/big.orig" "$R/big.c"
coderive k1.sh k2.sh '%s/^L0005$/L0005c/:%s/^L2100$/L2100c/:q!'
chg="$(sed -n '/^=== COMPAT PATCH ===$/,/^=== END /p' "$R/new.sh" |
       grep -c '^[-+][^-+]' || true)"
if [ "$chg" -ge 4 ] && [ "$chg" -le 12 ]; then
	ok "compat: the differ splits a huge span instead of rewriting it"
else
	fail "compat: the differ splits a huge span instead of rewriting it"
	echo "    changed lines in COMPAT PATCH=$chg (expected 4)"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /' | head -3
fi

cp "$R/big.orig" "$R/big.c"
stack k1.sh new.sh
if [ "$(sed -n '6p;1101p;2101p' "$R/big.c")" = "$(printf 'L0005c\nL1100t\nL2100c')" ]; then
	ok "compat: a split-span compat patch applies on an origin tree"
else
	fail "compat: a split-span compat patch applies on an origin tree"
	sed -n '6p;1101p;2101p' "$R/big.c" | sed 's/^/    /'
fi

# Patterns are case-sensitive: nextvi's ignorecase defaults ON, so a one-byte
# substitution like s/x/w/ meant for "xrows" would land on the "X" of "MAX("
# earlier in the line. The emitted prologue turns it off ("ic 0"); without that
# this tree comes out with "MAw(" and an untouched "xrows".
printf 'A1\n\tfor (k = MAX(0, -t); k < xrows; k++)\nA3\n' > "$R/cs.orig"
printf -- '--- a/cs.c\n+++ b/cs.c\n@@ -1,3 +1,4 @@\n A1\n+CSPROBE\n \tfor (k = MAX(0, -t); k < xrows; k++)\n A3\n' > "$R/n1.diff"
printf -- '--- a/cs.c\n+++ b/cs.c\n@@ -1,3 +1,3 @@\n A1\n \tfor (k = MAX(0, -t); k < xrows; k++)\n-A3\n+A3t\n' > "$R/n2.diff"
"$R_P2VI" -r "$R/n1.diff" > "$R/n1.sh"
"$R_P2VI" -r "$R/n2.diff" > "$R/n2.sh"
cp "$R/cs.orig" "$R/cs.c"
coderive n1.sh n2.sh '%s/xrows/wrows/:q!'
cp "$R/cs.orig" "$R/cs.c"
stack n1.sh new.sh
if [ "$(sed -n '3p' "$R/cs.c")" = "$(printf '\tfor (k = MAX(0, -t); k < wrows; k++)')" ]; then
	ok "compat: a one-byte substitution is matched case-sensitively"
else
	fail "compat: a one-byte substitution is matched case-sensitively"
	sed 's/^/    /' "$R/cs.c"
fi

# A regen must round-trip a multi-file block: one region back out, byte-identically.
cp "$R/ph.orig" "$R/ph.c"
cp "$R/pi.orig" "$R/pi.c"
dregen mf.sh
if cmp -s "$R/mf.sh" "$R/dregen.sh"; then
	ok "compat: a regen round-trips a two-file compat block byte-identically"
else
	fail "compat: a regen round-trips a two-file compat block byte-identically"
	diff "$R/mf.sh" "$R/dregen.sh" 2>&1 | sed 's/^/    /' | head
fi

# Buffer numbering survives a regeneration even when the block's file set is
# not the host's. Storage puts the compat regions before === PATCH2VI PATCH ===,
# so a regen parses the block's files first and a files[]-ordered b<N> would open
# them in the other order: here the host edits ph.c and pi.c while the block
# edits pi.c alone, so every b<N> in the emitted driver would shift by one.
cp "$R/ph.orig" "$R/ph.c"
cp "$R/pi.orig" "$R/pi.c"
coderive g1.sh g2.sh ':e pi.c:%s/^I3$/I3d/:q!'
cp "$R/new.sh" "$R/sf.sh"
cp "$R/ph.orig" "$R/ph.c"
cp "$R/pi.orig" "$R/pi.c"
dregen sf.sh
if [ -s "$R/sf.sh" ] && cmp -s "$R/sf.sh" "$R/dregen.sh"; then
	ok "compat: a regen keeps the buffer order of a block over one host file"
else
	fail "compat: a regen keeps the buffer order of a block over one host file"
	diff "$R/sf.sh" "$R/dregen.sh" 2>&1 | sed 's/^/    /' | head
fi

# rebuild the -C script (the earlier -C run clobbered new.sh)
printf 'L1\nL2\nL3\n' > "$R/draw.orig"
printf -- '--- a/draw.c\n+++ b/draw.c\n@@ -1,3 +1,4 @@\n L1\n+PROBE\n L2\n L3\n' > "$R/x1.diff"
printf -- '--- a/draw.c\n+++ b/draw.c\n@@ -1,3 +1,3 @@\n L1\n L2\n-L3\n+L3x\n' > "$R/x2.diff"
"$R_P2VI" -r "$R/x1.diff" > "$R/x1.sh"
"$R_P2VI" -r "$R/x2.diff" > "$R/x2.sh"
cp "$R/draw.orig" "$R/draw.c"
coderive x1.sh x2.sh '%s/^L2$/L2c/:q!'
cp "$R/new.sh" "$R/pr.sh"

cp "$R/draw.orig" "$R/draw.c"
dregen pr.sh
if diff "$R/pr.sh" "$R/dregen.sh" >/dev/null 2>&1; then
	ok "compat: a regen round-trips a stored two-block script byte-identically"
else
	fail "compat: a regen round-trips a stored two-block script byte-identically"
	diff "$R/pr.sh" "$R/dregen.sh" | sed 's/^/    /' | head -20
fi

# Re-running -C on a script that already carries a post block appends a NEW
# block at the group tail (existing block untouched); the user reshapes a
# different line. The origin+target replay applies the host (L3 -> L3x) and the
# stored post block (L2 -> L2c), so the new edit targets the post-replay line L3x.
cp "$R/draw.orig" "$R/draw.c"
coderive x1.sh pr.sh '%s/^L3x$/L3z/:q!'
if [ "$(grep -c '^=== PATCH2VI COMPAT [0-9]* src=' "$R/new.sh")" = 2 ] &&
   grep -q '^+L2c$' "$R/new.sh" && grep -q '^+L3z$' "$R/new.sh"; then
	ok "compat: re-running -C stacks a new block, keeps the existing one"
else
	fail "compat: re-running -C stacks a new block, keeps the existing one"
	grep -n 'PATCH2VI COMPAT' "$R/new.sh" | sed 's/^/    /'
fi

# The stacked script fires both compat units plus the host on an origin tree,
# and only the host on a clean tree (both gates quit before any edit).
cp "$R/new.sh" "$R/stack.sh"
cp "$R/draw.orig" "$R/draw.c"
stack x1.sh stack.sh
origin_ok=0
[ "$(sed -n 1,3p "$R/draw.c")" = "$(printf 'L1\nPROBE\nL2c')" ] && origin_ok=1
cp "$R/draw.orig" "$R/draw.c"
( cd "$R" && VI="$VI" sh stack.sh ) >/dev/null 2>&1
if [ "$origin_ok" = 1 ] && [ "$(cat "$R/draw.c")" = "$(printf 'L1\nL2\nL3x')" ]; then
	ok "compat: stacked script fires on origin, gates no-op on clean"
else
	fail "compat: stacked script fires on origin, gates no-op on clean"
	sed 's/^/    /' "$R/draw.c"
fi

# Post stacking: re-running -C on a target that already carries a post block
# derives the NEW block on top of that block's output. The origin+target replay
# applies the stored post block (L2 -> L2c), so L2c exists before handover; the
# new session edits L2c -> L2cc. Without replaying the target's own blocks the
# buffer would still read L2, the edit would match nothing, and -C would fail
# with "no compat derived" / empty output.
cp "$R/draw.orig" "$R/draw.c"
coderive x1.sh pr.sh '%s/^L2c$/L2cc/:q!'
if [ -s "$R/new.sh" ] &&
   [ "$(grep -c '^=== PATCH2VI COMPAT [0-9]* src=' "$R/new.sh")" = 2 ] &&
   grep -q '^-L2c$' "$R/new.sh" && grep -q '^+L2cc$' "$R/new.sh"; then
	ok "compat: -C stacks the new block atop the existing post block"
else
	fail "compat: -C stacks the new block atop the existing post block"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
	grep -n 'PATCH2VI COMPAT\|^[-+]L2' "$R/new.sh" 2>/dev/null | sed 's/^/    /'
fi

# The doubly-stacked script applies all three units in order on an origin tree:
# the host (L3 -> L3x), post block 1 (L2 -> L2c), post block 2 (L2c -> L2cc).
cp "$R/new.sh" "$R/stack2.sh"
cp "$R/draw.orig" "$R/draw.c"
stack x1.sh stack2.sh
if [ "$(cat "$R/draw.c")" = "$(printf 'L1\nPROBE\nL2cc\nL3x')" ]; then
	ok "compat: post stack applies all blocks in order on an origin tree"
else
	fail "compat: post stack applies all blocks in order on an origin tree"
	sed 's/^/    /' "$R/draw.c"
fi

# A regen reproduces a multi-block script byte-identically: every stored region
# is read back and re-emitted, host block and both compat blocks alike. Build a
# two-block script first, then round-trip it.
cp "$R/draw.orig" "$R/draw.c"
coderive x1.sh x2.sh '%s/^L2$/L2c/:q!'
cp "$R/new.sh" "$R/pr.sh"
cp "$R/draw.orig" "$R/draw.c"
coderive x1.sh pr.sh '%s/^L3x$/L3z/:q!'	# stacks a 2nd pre block
cp "$R/new.sh" "$R/two.sh"
if [ "$(grep -c '^=== PATCH2VI COMPAT [0-9]*' "$R/two.sh")" = 2 ]; then
	cp "$R/draw.orig" "$R/draw.c"
	dregen two.sh
	if diff "$R/two.sh" "$R/dregen.sh" >/dev/null 2>&1; then
		ok "compat: a regen reproduces a two-block script byte-identically"
	else
		fail "compat: a regen reproduces a two-block script byte-identically"
		diff "$R/two.sh" "$R/dregen.sh" | sed 's/^/    /' | head
	fi
else
	fail "compat: a regen reproduces a two-block script byte-identically"
	echo "    could not build a two-block script"
fi

# Mixed origins, the subset matrix. Two independent origins over one file: A
# inserts PA at the top, B inserts PB at the bottom; the target's own hunk is a
# third, disjoint line. A's compat block is derived first, B's on top of it, so
# a single script carries two blocks with two different origins. Each of the
# four trees (none / A / B / A+B) must run exactly the blocks whose origin is
# present - the case a shared gate tag gets wrong, since an absent origin's
# block then fires off the other's sensor.
printf 'L1\nL2\nL3\nL4\n' > "$R/m.orig"
printf -- '--- a/m.c\n+++ b/m.c\n@@ -1,4 +1,5 @@\n L1\n+PA\n L2\n L3\n L4\n' > "$R/a1.diff"
printf -- '--- a/m.c\n+++ b/m.c\n@@ -1,4 +1,5 @@\n L1\n L2\n L3\n L4\n+PB\n' > "$R/b1.diff"
printf -- '--- a/m.c\n+++ b/m.c\n@@ -1,4 +1,4 @@\n L1\n L2\n-L3\n+L3x\n L4\n' > "$R/mx.diff"
"$R_P2VI" -r "$R/a1.diff" > "$R/a1.sh"
"$R_P2VI" -r "$R/b1.diff" > "$R/b1.sh"
"$R_P2VI" -r "$R/mx.diff" > "$R/mx.sh"
cp "$R/m.orig" "$R/m.c"
coderive a1.sh mx.sh '%s/^L2$/L2a/:q!'		# block from origin A
cp "$R/new.sh" "$R/mix1.sh"
cp "$R/m.orig" "$R/m.c"
coderive b1.sh mix1.sh '%s/^L4$/L4b/:q!'	# block from origin B, stacked
cp "$R/new.sh" "$R/mix2.sh"
mixrun() {	# <origin scripts> -> tree of m.c after mix2.sh, '|'-joined
	cp "$R/m.orig" "$R/m.c"
	if [ -z "$1" ]; then
		( cd "$R" && chmod +x mix2.sh && VI="$VI" sh mix2.sh ) >/dev/null 2>&1
	else
		( cd "$R" && chmod +x $1 mix2.sh && VI="$VI" sh $1 mix2.sh ) >/dev/null 2>&1
	fi
	tr '\n' '|' < "$R/m.c"
}
if [ "$(grep -c '^=== PATCH2VI COMPAT [0-9]* src=' "$R/mix2.sh" 2>/dev/null)" = 2 ] &&
   grep -q '^# Compat [0-9]* src=a1.sh' "$R/mix2.sh" &&
   grep -q '^# Compat [0-9]* src=b1.sh' "$R/mix2.sh"; then
	ok "compat: two origins stack into one script"
else
	fail "compat: two origins stack into one script"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /'
fi
t_none="$(mixrun '')"
t_a="$(mixrun 'a1.sh')"
t_b="$(mixrun 'b1.sh')"
t_ab="$(mixrun 'a1.sh b1.sh')"
if [ "$t_none" = 'L1|L2|L3x|L4|' ] && [ "$t_a" = 'L1|PA|L2a|L3x|L4|' ] &&
   [ "$t_b" = 'L1|L2|L3x|L4b|PB|' ] && [ "$t_ab" = 'L1|PA|L2a|L3x|L4b|PB|' ]; then
	ok "compat: mixed origins fire per subset (none/A/B/A+B)"
else
	fail "compat: mixed origins fire per subset (none/A/B/A+B)"
	echo "    none=[$t_none] A=[$t_a] B=[$t_b] A+B=[$t_ab]"
fi

# A file only a compat block touches is written by that block's own body, not
# by the driver's write tail. The origin CREATES n.c, so a run on a tree the
# origin is missing from must leave no n.c at all: the gate misses, nothing
# edits the buffer, and a tail write would still put an empty file into a tree
# that never had one. With the origin applied the block writes it as usual.
printf -- '--- /dev/null\n+++ b/n.c\n@@ -0,0 +1,3 @@\n+N1\n+N2\n+N3\n' > "$R/n.diff"
"$R_P2VI" -r "$R/n.diff" > "$R/n.sh"
cp "$R/m.orig" "$R/m.c"; rm -f "$R/n.c"	# pre-origin tree: n.c does not exist
coderive n.sh mx.sh 'b0:%s/^N2$/N2c/:q!'	# b0 is the origin's new file
cp "$R/new.sh" "$R/nf.sh"
cp "$R/m.orig" "$R/m.c"; rm -f "$R/n.c"
( cd "$R" && VI="$VI" sh nf.sh ) > "$R/nfout" 2>&1
nf_clean=0
[ ! -e "$R/n.c" ] && [ "$(tr '\n' '|' < "$R/m.c")" = 'L1|L2|L3x|L4|' ] && nf_clean=1
# the sensors and the rewind address that empty buffer: silenced by err 0,
# and by nothing else - the host's own rewind is over a file that exists
nf_noise="$(tr -d '\r' < "$R/nfout" | grep -c 'invalid range' || true)"
nf_wrap="$(grep -o 'err 0' "$R/nf.sh" | wc -l | tr -d ' ')"
cp "$R/m.orig" "$R/m.c"; rm -f "$R/n.c"
stack n.sh nf.sh
nf_orig="$(tr '\n' '|' < "$R/n.c" 2>/dev/null)"
rm -f "$R/n.c"
if [ "$nf_clean" = 1 ] && [ "$nf_orig" = 'N1|N2c|N3|' ] && [ "$nf_noise" = 0 ] &&
   [ "$nf_wrap" -ge 1 ]; then
	ok "compat: an origin's new file is written only when the origin is"
else
	fail "compat: an origin's new file is written only when the origin is"
	echo "    clean=$nf_clean origin=[$nf_orig] noise=$nf_noise wrap=$nf_wrap"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /' | head -3
fi

# A fix that only a STACK of patches needs: -C repeats, one per origin, and the
# block gates on all of their landings at once. The two origins land in disjoint
# regions of one file, so each derives its own probes off its own snapshot pair;
# the cluster cut is transposed across them, so every cluster names both and no
# cluster can fire on one origin alone. A tree carrying only A, or only B, is
# not the tree the fix was written for.
coderive2() {	# <o1.sh> <o2.sh> <target.sh> <P2VI_EX>: -C twice into $R/new.sh
	rm -f "$R/new.sh"
	pty "P2VI_EX=$4" \
		"sh -c 'cd $R && $R_P2VI -C $1 -C $2 $3 > $R/new.sh 2>$R/nerr'" \
		> /dev/null 2>&1
}
i=1
: > "$R/mo.orig"
while [ $i -le 60 ]; do printf 'M%d\n' "$i" >> "$R/mo.orig"; i=$((i + 1)); done
sed -e 's/^M3$/M3a/' -e 's/^M8$/M8a/' -e 's/^M13$/M13a/' -e 's/^M18$/M18a/' \
    -e 's/^M23$/M23a/' "$R/mo.orig" > "$R/mo.a"
sed -e 's/^M28$/M28b/' -e 's/^M33$/M33b/' -e 's/^M38$/M38b/' \
    -e 's/^M43$/M43b/' -e 's/^M48$/M48b/' "$R/mo.orig" > "$R/mo.b"
sed 's/^M55$/M55t/' "$R/mo.orig" > "$R/mo.t"
for v in a b t; do
	diff -u "$R/mo.orig" "$R/mo.$v" |
		sed -e '1s|.*|--- a/mo.c|' -e '2s|.*|+++ b/mo.c|' > "$R/m$v.diff"
	"$R_P2VI" -r "$R/m$v.diff" > "$R/m$v.sh"
done
cp "$R/mo.orig" "$R/mo.c"	# pre-origin tree the replay reads
coderive2 ma.sh mb.sh mt.sh '%s/^M60$/M60c/:q!'
dregen new.sh
# one src= field per origin, so the label is read the way a reader splits it:
# every field of the header whose name is src=, in order
m_src="$(awk '/^=== PATCH2VI COMPAT /{for (i = 1; i <= NF; i++)
	if (substr($i, 1, 4) == "src=") printf "%s%s", n++ ? " " : "", substr($i, 5)
	print ""}' "$R/dregen.sh")"
if [ "$m_src" = 'ma.sh mb.sh' ]; then
	ok "compat: -C repeats, and the block names both origins"
else
	fail "compat: -C repeats, and the block names both origins"
	echo "    src=[$m_src]"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /' | head -3
fi
mrun() {	# <origin scripts> <script> -> last line of mo.c after <script>
	cp "$R/mo.orig" "$R/mo.c"
	if [ -z "$1" ]; then
		( cd "$R" && chmod +x "$2" && VI="$VI" sh "$2" ) >/dev/null 2>&1
	else
		( cd "$R" && chmod +x $1 "$2" && VI="$VI" sh $1 "$2" ) >/dev/null 2>&1
	fi
	tail -n 1 "$R/mo.c"
}
# The block demands BOTH origins: a tree carrying only A, or only B, is not the
# tree the fix was written for (a two-src= label is an AND under name matching).
m_both="$(mrun 'ma.sh mb.sh' new.sh)"
m_a="$(mrun 'ma.sh' new.sh)"
m_b="$(mrun 'mb.sh' new.sh)"
m_none="$(mrun '' new.sh)"
if [ "$m_both" = 'M60c' ] && [ "$m_a" = 'M60' ] && [ "$m_b" = 'M60' ] &&
   [ "$m_none" = 'M60' ]; then
	ok "compat: a two-origin block fires on both origins, never on one"
else
	fail "compat: a two-origin block fires on both origins, never on one"
	echo "    both=[$m_both] a=[$m_a] b=[$m_b] clean=[$m_none]"
fi


# -oC clusters the top-level -o into -C: no FILE of its own, the derived
# block lands back on the target script it extends (the same rule -oE follows),
# and nothing goes to stdout.
printf 'P1\nP2\nP3\n' > "$R/p.orig"
printf -- '--- a/p.c\n+++ b/p.c\n@@ -1,3 +1,4 @@\n P1\n+PROBE\n P2\n P3\n' > "$R/o1.diff"
printf -- '--- a/p.c\n+++ b/p.c\n@@ -1,3 +1,3 @@\n P1\n P2\n-P3\n+P3x\n' > "$R/o2.diff"
"$R_P2VI" -r "$R/o1.diff" > "$R/o1.sh"
"$R_P2VI" -r "$R/o2.diff" > "$R/o2.sh"
cp "$R/o2.sh" "$R/o2.keep"
cp "$R/p.orig" "$R/p.c"
rm -f "$R/ostdout"
pty 'P2VI_EX=%s/^P2$/P2c/:q!' \
	"sh -c 'cd $R && $R_P2VI -oC o1.sh o2.sh > $R/ostdout 2>$R/nerr'" \
	> /dev/null 2>&1
if [ ! -s "$R/ostdout" ] &&
   grep -q '^# Compat [0-9]* src=o1.sh' "$R/o2.sh" 2>/dev/null; then
	ok "compat: -oC updates the target script in place"
else
	fail "compat: -oC updates the target script in place"
	tr -d '\r' < "$R/nerr" | sed 's/^/    /' | head -3
fi
cp "$R/p.orig" "$R/p.c"
stack o1.sh o2.sh
p_full="$(tr '\n' '|' < "$R/p.c")"
cp "$R/p.orig" "$R/p.c"
( cd "$R" && VI="$VI" sh o2.sh ) >/dev/null 2>&1
p_clean="$(tr '\n' '|' < "$R/p.c")"
if [ "$p_full" = 'P1|PROBE|P2c|P3x|' ] && [ "$p_clean" = 'P1|P2|P3x|' ]; then
	ok "compat: the in-place updated script fires and no-ops like any other"
else
	fail "compat: the in-place updated script fires and no-ops like any other"
	echo "    origin=[$p_full] clean=[$p_clean]"
fi
# a file literally named "co" is still reachable through a separate argument
rm -f "$R/co"
"$R_P2VI" -r -o "$R/co" "$R/o2.diff" >/dev/null 2>&1
if [ -s "$R/co" ] && cmp -s "$R/co" "$R/o2.keep"; then
	ok "compat: -o co still names a file, not a -C cluster"
else
	fail "compat: -o co still names a file, not a -C cluster"
fi

# -E updates the base patch and carries the stored compat blocks over untouched:
# a base edit is the one thing that must not cost the blocks stacked on it. The
# replay runs with an empty applied set, so no gate fires into the host patch,
# and what the blocks are worth on the new base is left to a replay of each
# src= stack - hence the "unverified" the run says so with.
printf 'E1\nE2\nE3\n' > "$R/e.orig"
printf -- '--- a/e.c\n+++ b/e.c\n@@ -1,3 +1,4 @@\n E1\n+EPROBE\n E2\n E3\n' \
	> "$R/e1.diff"
printf -- '--- a/e.c\n+++ b/e.c\n@@ -1,3 +1,3 @@\n E1\n E2\n-E3\n+E3x\n' \
	> "$R/e2.diff"
"$R_P2VI" -r "$R/e1.diff" > "$R/e1.sh"
"$R_P2VI" -r "$R/e2.diff" > "$R/e2.sh"
cp "$R/e.orig" "$R/e.c"
rm -f "$R/new.sh"
pty 'P2VI_EX=%s/^E2$/E2c/:q!' \
	"sh -c 'cd $R && $R_P2VI -C e1.sh e2.sh > $R/ec.sh 2>$R/nerr'" \
	> /dev/null 2>&1

# amend_E <tree-setup-file> <out.sh> [P2VI_PATCH]: replay ec.sh over the tree
# and re-emit. The applied set is asserted via $3 when the origin is present.
amend_E() {
	cp "$1" "$R/e.c"
	pty 'P2VI_EX=q!' \
		"sh -c 'cd $R && ${3:+P2VI_PATCH=\"$3\" }$R_P2VI -E ec.sh > $R/$2 2>$R/eerr'" > /dev/null 2>&1
}

# blocks <script>: its stored compat regions, host patch excluded
blocks() {
	awk '/^=== PATCH2VI COMPAT /{p=1} /^=== PATCH2VI PATCH ===/{p=0} p' "$1"
}

# clean tree: the gate self-skips during the replay, so the compat edit never
# happens and the update carries the target's hunk alone - with the block still
# stored, byte for byte as the script shipped it
amend_E "$R/e.orig" "eclean.sh"
blocks "$R/ec.sh" > "$R/eb.old"
blocks "$R/eclean.sh" > "$R/eb.new"
if grep -q 'unverified' "$R/eerr" 2>/dev/null &&
   [ -s "$R/eb.new" ] && cmp -s "$R/eb.old" "$R/eb.new"; then
	ok "compat: -E carries a stored compat block over untouched"
else
	fail "compat: -E carries a stored compat block over untouched"
	tr -d '\r' < "$R/eerr" | sed 's/^/    /' | head -3
fi
cp "$R/e.orig" "$R/e.c"
( cd "$R" && VI="$VI" sh eclean.sh ) >/dev/null 2>&1
if [ "$(cat "$R/e.c")" = "$(printf 'E1\nE2\nE3x')" ]; then
	ok "compat: -E over a clean tree folds nothing, keeps the host hunk"
else
	fail "compat: -E over a clean tree folds nothing, keeps the host hunk"
	sed 's/^/    /' "$R/e.c"
fi

# origin tree: $P2VI_PATCH names e1.sh, but -E empties the applied set for its
# own replay - a block that fired would land in the buffers and be re-derived
# into the host patch while the block itself stays stored and gated, applying
# twice on the next run. So the update is the base over this tree and nothing
# else, and the block still needs its origin to do anything.
printf 'E1\nEPROBE\nE2\nE3\n' > "$R/e.probe"
amend_E "$R/e.probe" "eorig.sh" e1.sh
blocks "$R/eorig.sh" > "$R/eb.env"
cp "$R/e.probe" "$R/e.c"
( cd "$R" && VI="$VI" sh eorig.sh ) >/dev/null 2>&1
ungated=$(cat "$R/e.c")
cp "$R/e.probe" "$R/e.c"
( cd "$R" && VI="$VI" P2VI_PATCH="e1.sh" sh eorig.sh ) >/dev/null 2>&1
if cmp -s "$R/eb.old" "$R/eb.env" &&
   [ "$ungated" = "$(printf 'E1\nEPROBE\nE2\nE3x')" ] &&
   [ "$(cat "$R/e.c")" = "$(printf 'E1\nEPROBE\nE2c\nE3x')" ]; then
	ok "compat: -E folds no fired block in, whatever \$P2VI_PATCH says"
else
	fail "compat: -E folds no fired block in, whatever \$P2VI_PATCH says"
	sed 's/^/    /' "$R/e.c"
fi

# Naming a block's section register is the other half of -E: that one block is
# rebuilt and everything else in the script stands. The origins come from the
# block's own src= label, are looked for beside the target and replayed ahead
# of it, so the gate fires exactly as the chain would make it; the baseline is
# taken just before the block runs, which is why a session that changes nothing
# re-derives the diff already stored.
cp "$R/e.orig" "$R/e.c"
rm -f "$R/ert.sh"
pty 'P2VI_EX=q!' \
	"sh -c 'cd $R && $R_P2VI -E ec.sh 231 > $R/ert.sh 2>$R/eerr'" >/dev/null 2>&1
if cmp -s "$R/ec.sh" "$R/ert.sh"; then
	ok "compat: -E reg round-trips the named block byte for byte"
else
	fail "compat: -E reg round-trips the named block byte for byte"
	tr -d '\r' < "$R/eerr" | sed 's/^/    /' | head -3
fi

# an edit in the handover joins the block, not the host patch: EPROBE is the
# origin's line, so it only exists in the tree the block is derived against
cp "$R/e.orig" "$R/e.c"
rm -f "$R/eup.sh"
pty 'P2VI_EX=%s/^EPROBE$/EDITED/:q!' \
	"sh -c 'cd $R && $R_P2VI -E ec.sh 231 > $R/eup.sh 2>$R/eerr'" >/dev/null 2>&1
if sed -n '/=== COMPAT PATCH ===/,/=== END ===/p' "$R/eup.sh" | grep -q '^+EDITED$' &&
   [ "$(sed -n '/=== PATCH2VI PATCH ===/,$p' "$R/eup.sh")" = \
     "$(sed -n '/=== PATCH2VI PATCH ===/,$p' "$R/ec.sh")" ]; then
	ok "compat: -E reg rebuilds one block and leaves the host patch alone"
else
	fail "compat: -E reg rebuilds one block and leaves the host patch alone"
	tr -d '\r' < "$R/eerr" | sed 's/^/    /' | head -3
fi
cp "$R/e.orig" "$R/e.c"
stack e1.sh eup.sh
e_full="$(tr '\n' '|' < "$R/e.c")"
cp "$R/e.orig" "$R/e.c"
( cd "$R" && VI="$VI" sh eup.sh ) >/dev/null 2>&1
e_clean="$(tr '\n' '|' < "$R/e.c")"
if [ "$e_full" = 'E1|EDITED|E2c|E3x|' ] && [ "$e_clean" = 'E1|E2|E3x|' ]; then
	ok "compat: the rebuilt block still fires only under its own origin"
else
	fail "compat: the rebuilt block still fires only under its own origin"
	echo "    origin=[$e_full] clean=[$e_clean]"
fi

# a register no block carries is refused, and nothing is written
cp "$R/e.orig" "$R/e.c"
rm -f "$R/ebad.sh"
pty 'P2VI_EX=q!' \
	"sh -c 'cd $R && $R_P2VI -E ec.sh 999 > $R/ebad.sh 2>$R/eerr'" >/dev/null 2>&1
if [ ! -s "$R/ebad.sh" ] && grep -q 'no compat block on register 999' "$R/eerr"; then
	ok "compat: -E reg refuses a register no block carries"
else
	fail "compat: -E reg refuses a register no block carries"
	tr -d '\r' < "$R/eerr" | sed 's/^/    /' | head -3
fi

# FAILURE PLACEMENT. A QF2=1 run reports every miss and keeps going, so what
# the missed hunks were meant to do is left only in the scrollback. The report
# chain logs each FAIL line to a register, and the mark it names ("...:m<id>")
# finds the edit itself in the command stream the block ran, so -E puts it back
# at the line the FAIL reported: run there when that cannot lose anything (an
# insert, or a substitute, which has to match first), otherwise dropped in as a
# ">>> p2v FAIL" block to apply by hand.
printf 'F1\nF2\nF3\nF4\nF5\nF6\n' > "$R/f.orig"
printf -- '--- a/f.c\n+++ b/f.c\n@@ -1,6 +1,6 @@\n F1\n-F2\n+F2x\n F3\n F4\n F5\n-F6\n+F6y\n' \
	> "$R/f.diff"
"$R_P2VI" -r "$R/f.diff" > "$R/f.sh"

# amend_F <out.sh>: replay f.sh over f.c with QF2=1 and re-emit
amend_F() {
	pty 'P2VI_EX=q!' \
		"sh -c 'cd $R && QF2=1 $R_P2VI -E f.sh > $R/$1 2>$R/ferr'" > /dev/null 2>&1
}

# nothing missed: the log stays empty and the update is the plain one
cp "$R/f.orig" "$R/f.c"
amend_F "fok.sh"
if [ -s "$R/fok.sh" ] && ! grep -q 'p2v FAIL' "$R/fok.sh" &&
   ! tr -d '\r' < "$R/ferr" | grep -q 'put back'; then
	ok "placement: a run that missed nothing places nothing"
else
	fail "placement: a run that missed nothing places nothing"
	tr -d '\r' < "$R/ferr" | sed 's/^/    /' | head -3
fi

# the anchors of both groups are gone, but the reported line of the second one
# still holds a "6": that substitute is re-aimed and applies, the first cannot
# match and is left in place with its command
printf 'AA\nBB\nCC\nDD\nEE\nq6q\n' > "$R/f.c"
amend_F "ffail.sh"
if tr -d '\r' < "$R/ferr" | grep -q '2 failed hunks put back' &&
   grep -q "^+>>> p2v FAIL f.c:2:m1$" "$R/ffail.sh" &&
   grep -q "^+'1s/2/2x/$" "$R/ffail.sh" &&
   grep -q '^+<<< p2v END$' "$R/ffail.sh" &&
   grep -q '^+q6yq$' "$R/ffail.sh"; then
	ok "placement: a missed hunk is put back, a re-aimable one applied"
else
	fail "placement: a missed hunk is put back, a re-aimable one applied"
	tr -d '\r' < "$R/ferr" | sed 's/^/    /' | head -3
	sed -n '/PATCH2VI PATCH/,$p' "$R/ffail.sh" | sed 's/^/    /'
fi

# a delete is never run at a guessed line - it would eat a line the patch never
# named - so it always arrives as a block, and the session is parked on the
# first one (the file the handover writes holds the line the cursor is on)
printf -- '--- a/g.c\n+++ b/g.c\n@@ -1,4 +1,3 @@\n G1\n G2\n-G3\n G4\n' > "$R/g.diff"
"$R_P2VI" -r "$R/g.diff" > "$R/g.sh"
printf 'G1\nZZ\nYY\nXX\n' > "$R/g.c"
pty 'P2VI_EX=q!' \
	"sh -c 'cd $R && QF2=1 $R_P2VI -E g.sh > $R/gfail.sh 2>$R/gerr'" \
	> /dev/null 2>&1
# again, this time writing the line the cursor sits on out of the session (a
# run of its own: the write would otherwise show up as a buffer to diff)
printf 'G1\nZZ\nYY\nXX\n' > "$R/g.c"
rm -f "$R/gcur"
pty "P2VI_EX=.w! $R/gcur:q!" \
	"sh -c 'cd $R && QF2=1 $R_P2VI -E g.sh > /dev/null 2>&1'" > /dev/null 2>&1
if grep -q "^+>>> p2v FAIL g.c:3:m1$" "$R/gfail.sh" &&
   grep -q "^+'1d$" "$R/gfail.sh" &&
   [ "$(cat "$R/gcur" 2>/dev/null)" = '>>> p2v FAIL g.c:3:m1' ]; then
	ok "placement: a delete is never guessed at, and parks the cursor"
else
	fail "placement: a delete is never guessed at, and parks the cursor"
	tr -d '\r' < "$R/gerr" | sed 's/^/    /' | head -3
	sed -n '/PATCH2VI PATCH/,$p' "$R/gfail.sh" | sed 's/^/    /'
fi


echo ""
echo "=== applied-set (env chain) tests ==="

# The fixtures: two origins with DISJOINT landings (CA after S1, CB after S3),
# so each applies cleanly over any subset of the other; the target edits a
# middle line whose context survives both; the compat block (gated on BOTH
# origins) edits a line nobody else touches.
printf 'S1\nS2\nS3\nS4\n' > "$R/cn.orig"
printf -- '--- a/cn.c\n+++ b/cn.c\n@@ -1,3 +1,4 @@\n S1\n+CA\n S2\n S3\n' > "$R/ca.diff"
printf -- '--- a/cn.c\n+++ b/cn.c\n@@ -2,3 +2,4 @@\n S2\n S3\n+CB\n S4\n' > "$R/cb.diff"
printf -- '--- a/cn.c\n+++ b/cn.c\n@@ -1,3 +1,3 @@\n S1\n-S2\n+S2x\n S3\n' > "$R/cx.diff"
"$R_P2VI" -r "$R/ca.diff" > "$R/ca.sh"
"$R_P2VI" -r "$R/cb.diff" > "$R/cb.sh"
"$R_P2VI" -r "$R/cx.diff" > "$R/cx.sh"
# compat block gated on BOTH ca and cb (two -C), editing S4 -> S4c
cp "$R/cn.orig" "$R/cn.c"
pty 'P2VI_EX=%s/^S4$/S4c/:q!' \
	"sh -c 'cd $R && $R_P2VI -C ca.sh -C cb.sh cx.sh > $R/cn.sh 2>$R/cn.nerr'" \
	> /dev/null 2>&1

# Chain propagation and ordering: each script appends its own basename to
# $P2VI_PATCH before invoking the next, so the deepest script sees exactly the
# origins that ran before it and its block fires only when every origin it
# names is in the set.
cp "$R/cn.orig" "$R/cn.c"
stack ca.sh cb.sh cn.sh
cn_full="$(tr '\n' '|' < "$R/cn.c")"
cp "$R/cn.orig" "$R/cn.c"
stack ca.sh cn.sh
cn_a="$(tr '\n' '|' < "$R/cn.c")"
cp "$R/cn.orig" "$R/cn.c"
( cd "$R" && chmod +x cn.sh && VI="$VI" sh cn.sh ) >/dev/null 2>&1
cn_clean="$(tr '\n' '|' < "$R/cn.c")"
if [ "$cn_full" = 'S1|CA|S2x|S3|CB|S4c|' ] && [ "$cn_a" = 'S1|CA|S2x|S3|S4|' ] &&
   [ "$cn_clean" = 'S1|S2x|S3|S4|' ]; then
	ok "applied-set: a two-origin block fires in the chain, not under one origin"
else
	fail "applied-set: a two-origin block fires in the chain, not under one origin"
	echo "    full=[$cn_full] a=[$cn_a] clean=[$cn_clean]"
fi

# Basename normalization: ./ca.sh, cb.sh and /abs/path/cn.sh all put their
# basename in the set, and a stored src=ca.sh matches every spelling.
cp "$R/cn.orig" "$R/cn.c"
( cd "$R" && chmod +x ca.sh cb.sh cn.sh && VI="$VI" sh "./ca.sh" "cb.sh" "$R/cn.sh" ) >/dev/null 2>&1
cn_paths="$(tr '\n' '|' < "$R/cn.c")"
if [ "$cn_paths" = 'S1|CA|S2x|S3|CB|S4c|' ]; then
	ok "applied-set: argument paths normalize to basenames"
else
	fail "applied-set: argument paths normalize to basenames"
	echo "    got=[$cn_paths]"
fi

# Manual P2VI_PATCH inheritance: a tree that already carries the origins (applied
# by hand, or upstream) fires the block when the set says so, and the origin
# scripts are not re-run. Without the assertion the same tree no-ops the block.
cp "$R/cn.orig" "$R/cn.c"
stack ca.sh cb.sh				# both origins land, outside the chain
( cd "$R" && chmod +x cn.sh && VI="$VI" sh cn.sh ) >/dev/null 2>&1
cn_unasserted="$(tr '\n' '|' < "$R/cn.c")"
cp "$R/cn.orig" "$R/cn.c"
stack ca.sh cb.sh
( cd "$R" && P2VI_PATCH="ca.sh cb.sh" VI="$VI" sh cn.sh ) >/dev/null 2>&1
cn_hand="$(tr '\n' '|' < "$R/cn.c")"
if [ "$cn_unasserted" = 'S1|CA|S2x|S3|CB|S4|' ] &&
   [ "$cn_hand" = 'S1|CA|S2x|S3|CB|S4c|' ]; then
	ok "applied-set: P2VI_PATCH asserts origins the chain never ran"
else
	fail "applied-set: P2VI_PATCH asserts origins the chain never ran"
	echo "    quiet=[$cn_unasserted] asserted=[$cn_hand]"
fi

# A named origin is matched by basename from an absolute $P2VI_PATCH too.
cp "$R/cn.orig" "$R/cn.c"
stack ca.sh cb.sh
( cd "$R" && P2VI_PATCH="$R/ca.sh $R/cb.sh" VI="$VI" sh cn.sh ) >/dev/null 2>&1
cn_abs="$(tr '\n' '|' < "$R/cn.c")"
if [ "$cn_abs" = 'S1|CA|S2x|S3|CB|S4c|' ]; then
	ok "applied-set: P2VI_PATCH entries normalize to basenames"
else
	fail "applied-set: P2VI_PATCH entries normalize to basenames"
	echo "    got=[$cn_abs]"
fi

# Mid-chain failure: each script applies before recursing, so a failure in the
# deepest invocation leaves the earlier origins applied (the "&&" behaviour of
# the old form, now carried by the chain). An absent final script is a hard
# failure even though the first two have already run; the chain must not
# "apply" the missing one silently.
cp "$R/cn.orig" "$R/cn.c"
( cd "$R" && chmod +x ca.sh cb.sh && VI="$VI" sh ca.sh cb.sh nosuch.sh ) \
	>/dev/null 2>&1 || mid_rc=$?
mid_tree="$(tr '\n' '|' < "$R/cn.c")"
if [ "$mid_rc" != 0 ] && [ "$mid_tree" = 'S1|CA|S2|S3|CB|S4|' ]; then
	ok "applied-set: a mid-chain failure leaves the earlier origins applied"
else
	fail "applied-set: a mid-chain failure leaves the earlier origins applied"
	echo "    rc=$mid_rc tree=[$mid_tree]"
fi

# -e accumulates the same way the shell chain does: patch2vi -e ca.sh cb.sh
# cn.sh applies the compat block, and -e ca.sh cn.sh does not.
cp "$R/cn.orig" "$R/cn.c"
( cd "$R" && $R_P2VI -e ca.sh cb.sh cn.sh ) >/dev/null 2>&1 || true
cn_e="$(tr '\n' '|' < "$R/cn.c")"
cp "$R/cn.orig" "$R/cn.c"
( cd "$R" && $R_P2VI -e ca.sh cn.sh ) >/dev/null 2>&1 || true
cn_ea="$(tr '\n' '|' < "$R/cn.c")"
if [ "$cn_e" = 'S1|CA|S2x|S3|CB|S4c|' ] && [ "$cn_ea" = 'S1|CA|S2x|S3|S4|' ]; then
	ok "applied-set: -e accumulates its scripts the way the chain does"
else
	fail "applied-set: -e accumulates its scripts the way the chain does"
	echo "    both=[$cn_e] a=[$cn_ea]"
fi

# A chain without compat blocks still carries the set forward: each script
# applies itself and hands the rest on (the argument queue shrinks, the set
# grows), so a plain chain like lsp+visual+splits+incsearch runs every script
# in order exactly once.
printf 'N1\nN2\nN3\n' > "$R/pl.orig"
printf -- '--- a/pl.c\n+++ b/pl.c\n@@ -1,1 +1,2 @@\n N1\n+N1a\n' > "$R/pl1.diff"
printf -- '--- a/pl.c\n+++ b/pl.c\n@@ -3,1 +3,2 @@\n N3\n+N3a\n' > "$R/pl2.diff"
"$R_P2VI" -r "$R/pl1.diff" > "$R/pl1.sh"
"$R_P2VI" -r "$R/pl2.diff" > "$R/pl2.sh"
cp "$R/pl.orig" "$R/pl.c"
stack pl1.sh pl2.sh
pl_tree="$(tr '\n' '|' < "$R/pl.c")"
if [ "$pl_tree" = 'N1|N1a|N2|N3|N3a|' ]; then
	ok "applied-set: a no-compat chain applies every script exactly once"
else
	fail "applied-set: a no-compat chain applies every script exactly once"
	echo "    got=[$pl_tree]"
fi

# A script written by a patch2vi old enough to gate on stored probes still
# parses: its === GATE === regions are read past and dropped, never re-emitted.
# Splice one into cn.sh's storage the shape those scripts wrote it (right after
# the region header, before === COMPAT PATCH ===) and regenerate: the result is
# the unspliced script byte for byte, and it still fires on names alone.
awk '/^=== COMPAT PATCH ===$/ && !d {
	print "=== GATE 1 present tag 1000 probe cn.c ==="
	print "S1"
	print "=== END ==="
	d = 1
} { print }' "$R/cn.sh" > "$R/cg.sh"
cp "$R/cn.orig" "$R/cn.c"
dregen cg.sh
cp "$R/dregen.sh" "$R/cg2.sh"
cp "$R/cn.orig" "$R/cn.c"
stack ca.sh cb.sh cg2.sh
cg_tree="$(tr '\n' '|' < "$R/cn.c")"
if grep -q '^=== GATE ' "$R/cg.sh" && [ -s "$R/cg2.sh" ] &&
   ! grep -q '^=== GATE ' "$R/cg2.sh" && cmp -s "$R/cn.sh" "$R/cg2.sh" &&
   [ "$cg_tree" = 'S1|CA|S2x|S3|CB|S4c|' ]; then
	ok "applied-set: a stored gate region is dropped, not carried, on regen"
else
	fail "applied-set: a stored gate region is dropped, not carried, on regen"
	echo "    tree=[$cg_tree]"
	diff "$R/cn.sh" "$R/cg2.sh" | sed 's/^/    /' | head
	tr -d '\r' < "$R/derr" | sed 's/^/    /' | head -3
fi

# The gate is the editor's, not the shell's: the script hands $P2VI_PATCH to
# one register, in the same double-quoted word the DBG/QF switches live in, and
# does nothing else with it. No flag defaults, no case matches, no basename
# loop - so there is no second implementation of membership to drift.
if ! grep -q '^f2[0-9][0-9]=0$' "$R/cn.sh" &&
   ! grep -q 'case " \$applied' "$R/cn.sh" &&
   [ "$(grep -c '^"229reg  \$P2VI_PATCH ' "$R/cn.sh")" = 1 ]; then
	ok "applied-set: the set reaches ex as one register, not as shell flags"
else
	fail "applied-set: the set reaches ex as one register, not as shell flags"
	sed -n '/^f2/p;/case /p' "$R/cn.sh" | sed 's/^/    /' | head
fi

# Membership is by whole name: a set entry that merely contains an origin's
# name, or is contained by it, leaves the block cold. Only the exact basename
# (in any path spelling) fires it.
cp "$R/cn.orig" "$R/cn.c"
stack ca.sh cb.sh
( cd "$R" && P2VI_PATCH="xca.sh ca.shy cb.sh" VI="$VI" sh cn.sh ) >/dev/null 2>&1
cn_near="$(tr '\n' '|' < "$R/cn.c")"
if [ "$cn_near" = 'S1|CA|S2x|S3|CB|S4|' ]; then
	ok "applied-set: a near-miss name is not a member"
else
	fail "applied-set: a near-miss name is not a member"
	echo "    got=[$cn_near]"
fi

echo ""
echo "=== -aE absolute host tests ==="

# -a says absolute line numbers and -E must not quietly say otherwise. A base
# that never moves ships its host body as bare line numbers - a fraction of the
# size of the search rungs - while the stored compat blocks stay search
# anchored: a block only ever runs in a chain, where an origin has moved the
# lines by definition.
#
# sec_body <script> <suffix>: the staged body of section "$P2VIF".<suffix>,
# newlines and all - one shell "line" holds the whole body.
sec_body() {
	awk -v suf="$2" '
		/^printf .%s/ { buf = ""; inb = 1 }
		inb { buf = buf $0 "\n" }
		inb && $0 ~ ("> \"\\$P2VIF\"\\." suf "$") { printf "%s", buf; inb = 0 }
	' "$1"
}

# The fixture: an origin inserting a probe (the gate's landmark), a target
# whose TOPMOST edit is an insertion, and a block edit on a line nobody else
# touches. The insertion matters: absolute mode emits a file bottom-to-top, so
# it is the LAST command of the host body - the one position where a staged
# body's trailing newline reaches a file, as one more line of that command's
# argument.
printf 'A1\nA2\nA3\nA4\n' > "$R/ab.orig"
printf -- '--- a/ab.c\n+++ b/ab.c\n@@ -2,3 +2,4 @@\n A2\n A3\n A4\n+PROBE\n' > "$R/ao.diff"
printf -- '--- a/ab.c\n+++ b/ab.c\n@@ -1,4 +1,5 @@\n A1\n+NEW\n A2\n A3\n-A4\n+A4x\n' > "$R/at.diff"
"$R_P2VI" -r "$R/ao.diff" > "$R/ao.sh"
"$R_P2VI" -r "$R/at.diff" > "$R/at.sh"
cp "$R/ab.orig" "$R/ab.c"
pty 'P2VI_EX=%s/^A3$/A3c/:q!' \
	"sh -c 'cd $R && $R_P2VI -C ao.sh at.sh > $R/ab_blk.sh 2>$R/ab.nerr'" \
	> /dev/null 2>&1

# Both regens read the pre-image off disk, so each starts from the base tree
for ab_f in '-aE:ab_abs.sh' '-E:ab_rel.sh' '-arE:ab_ar.sh' '-raE:ab_ra.sh'; do
	cp "$R/ab.orig" "$R/ab.c"
	pty 'P2VI_EX=q!' "sh -c 'cd $R && $R_P2VI ${ab_f%%:*} ab_blk.sh > $R/${ab_f#*:} 2>$R/ab.nerr2'" \
		> /dev/null 2>&1
done

if ! sec_body "$R/ab_abs.sh" 0 | grep -q 'f> ' &&
   sec_body "$R/ab_abs.sh" 0 | grep -q '1i NEW' &&
   sec_body "$R/ab_rel.sh" 0 | grep -q 'f> '; then
	ok "-aE emits an absolute host body, -E alone stays relative"
else
	fail "-aE emits an absolute host body, -E alone stays relative"
	tr -d '\r' < "$R/ab.nerr2" | sed 's/^/    /'
fi

# The point of the mode: the host body is what the size goes into
if [ "$(wc -c < "$R/ab_abs.sh")" -lt "$(wc -c < "$R/ab_rel.sh")" ]; then
	ok "-aE ships a smaller script than -E"
else
	fail "-aE ships a smaller script than -E"
fi

# The blocks are not the host's to renumber: they come out of -aE exactly as
# -E emits them, searches and all, gate comment included
if [ "$(sec_body "$R/ab_abs.sh" 231)" = "$(sec_body "$R/ab_rel.sh" 231)" ] &&
   sec_body "$R/ab_abs.sh" 231 | grep -q 'f> ' &&
   grep -q '^# Compat 231 src=ao.sh' "$R/ab_abs.sh"; then
	ok "-aE keeps the stored blocks search anchored and gated"
else
	fail "-aE keeps the stored blocks search anchored and gated"
fi

# Last of -a/-r on the command line wins, and the -E that ends the cluster
# does not vote
if sec_body "$R/ab_ar.sh" 0 | grep -q 'f> ' &&
   ! sec_body "$R/ab_ra.sh" 0 | grep -q 'f> '; then
	ok "-arE is relative, -raE is absolute: last of -a/-r wins"
else
	fail "-arE is relative, -raE is absolute: last of -a/-r wins"
fi

# End to end on the base tree, where an absolute body is what it claims to be:
# the -aE script does what the -E one does, byte for byte. A staged body that
# ended in a newline would leave a stray blank line after NEW here, since the
# insert is the body's last command.
cp "$R/ab.orig" "$R/ab.c"
( cd "$R" && chmod +x ab_abs.sh && VI="$VI" sh ab_abs.sh ) >/dev/null 2>&1
ab_abs_tree="$(tr '\n' '|' < "$R/ab.c")"
cp "$R/ab.orig" "$R/ab.c"
( cd "$R" && chmod +x ab_rel.sh && VI="$VI" sh ab_rel.sh ) >/dev/null 2>&1
ab_rel_tree="$(tr '\n' '|' < "$R/ab.c")"
if [ "$ab_abs_tree" = 'A1|NEW|A2|A3|A4x|' ] &&
   [ "$ab_abs_tree" = "$ab_rel_tree" ]; then
	ok "-aE applies on the base tree exactly as -E does"
else
	fail "-aE applies on the base tree exactly as -E does"
	echo "    abs=[$ab_abs_tree] rel=[$ab_rel_tree]"
fi

# The mode's contract in the chain: an absolute host body is right as long as
# nothing ahead of it moves the lines it edits, which is what "-a" claims and
# all it claims. The origin here lands PROBE below every host edit, so the
# chain applies whole - the gate fires, the block edits A3 by search, and the
# host's own line numbers still point where they did.
cp "$R/ab.orig" "$R/ab.c"
stack ao.sh ab_abs.sh
ab_chain="$(tr '\n' '|' < "$R/ab.c")"
if [ "$ab_chain" = 'A1|NEW|A2|A3c|A4x|PROBE|' ]; then
	ok "-aE stacks with an origin that lands below its edits"
else
	fail "-aE stacks with an origin that lands below its edits"
	echo "    got=[$ab_chain]"
fi

fi

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"
[ $FAIL -eq 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED"
exit $FAIL
