#!/bin/sh
# Nextvi test suite - POSIX shell
# Tests ex commands and vi normal mode

PASS=0
FAIL=0
N=0
VI=./vi

check() {
	name="$1" expected="$2" actual="$3"
	N=$((N + 1))
	printf 'Test %d: "%s"\n' "$N" "$name"
	if [ "$expected" = "$actual" ]; then
		PASS=$((PASS + 1))
	else
		FAIL=$((FAIL + 1))
		printf 'FAIL\n  expected: |%s|\n  actual:   |%s|\n' \
			"$expected" "$actual"
	fi
}

check_exit() {
	name="$1" expected="$2" actual="$3"
	N=$((N + 1))
	printf 'Test %d: "%s"\n' "$N" "$name"
	if [ "$expected" = "$actual" ]; then
		PASS=$((PASS + 1))
	else
		FAIL=$((FAIL + 1))
		printf 'FAIL\n  expected exit: %s\n  actual exit:   %s\n' \
			"$expected" "$actual"
	fi
}

TMPFILE=$(mktemp /tmp/nextvi_test_XXXXXX)
OUTFILE=/tmp/nextvi_out_$$
trap 'rm -f "$TMPFILE" "$OUTFILE"' EXIT

# run_ex: pass EXINIT, capture stdout; buffer unchanged on disk
run_ex() {
	EXINIT="$1" "$VI" -sm "$TMPFILE" </dev/null 2>/dev/null
}

# run_vi: pass vi key sequence; write result to OUTFILE, read it back
run_vi() {
	rm -f "$OUTFILE"
	EXINIT=":& $1:w! $OUTFILE:q" "$VI" -e "$TMPFILE" </dev/null >/dev/null 2>&1
	cat "$OUTFILE" 2>/dev/null
}

# run_mac: pass EXINIT for ex+macro tests (-e mode); writes to OUTFILE
run_mac() {
	rm -f "$OUTFILE"
	EXINIT="$1" "$VI" -e "$TMPFILE" </dev/null >/dev/null 2>&1
	cat "$OUTFILE" 2>/dev/null
}

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':1p:q')
check 'print line 1' 'hello world' "$out"

printf 'line1\nline2\nline3\n' > "$TMPFILE"
out=$(run_ex ':1,3p:q')
check 'print range 1-3' "$(printf 'line1\nline2\nline3')" "$out"

printf 'aaa\nbbb\nccc\n' > "$TMPFILE"
out=$(run_ex ':%p:q')
check 'print all with %' "$(printf 'aaa\nbbb\nccc')" "$out"

printf 'first\nsecond\nthird\n' > "$TMPFILE"
out=$(run_ex ':$p:q')
check 'print last line ($)' 'third' "$out"

printf 'irrelevant\n' > "$TMPFILE"
out=$(run_ex ':p hello test:q')
check 'print literal arg' 'hello test' "$out"

printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':3=1:q')
check 'print line number (=)' '3' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':%s/world/nextvi/:%p:q!')
check 'substitute simple' 'hello nextvi' "$out"

printf 'int a; int b;\n' > "$TMPFILE"
out=$(run_ex ':%s/int/uint/g:%p:q!')
check 'substitute global' 'uint a; uint b;' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':%s/(hello) (world)/\2 \1/:%p:q!')
check 'substitute backreference' 'world hello' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':s/void/x/:??!p not found:q')
check 'conditional else on sub no-match' 'not found' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':s/world/x/:??p found:q!')
check 'conditional then on sub match' 'found' "$out"

printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ':2,4d:%p:q!')
check 'delete range 2-4' "$(printf 'a\ne')" "$out"

printf 'a\nb\nc\nd\n' > "$TMPFILE"
out=$(run_ex ':3,$d:%p:q!')
check 'delete to end' "$(printf 'a\nb')" "$out"

printf 'line1\n\nline2\n\nline3\n' > "$TMPFILE"
out=$(run_ex ':g/^$/d:%p:q!')
check 'delete empty lines (global)' "$(printf 'line1\nline2\nline3')" "$out"

printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':%-1j:%p:q!')
check 'join all no padding' 'abc' "$out"

printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':%-1jj:%p:q!')
check 'join all with space padding' 'a b c' "$out"

printf 'hello\nworld\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:$pu 97:%p:q!')
check 'yank line and paste at end' "$(printf 'hello\nworld\nhello')" "$out"

printf 'line1\nline2\nline3\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:2ya+ 97:1pu 97:%p:q!')
check 'append to register then paste' \
	"$(printf 'line1\nline1\nline2\nline2\nline3')" "$out"

printf 'line1\n' > "$TMPFILE"
out=$(run_ex ':97reg hello:$pu 97:%p:q!')
check 'put string into register via :reg' "$(printf 'line1\nhello')" "$out"

printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':g/int/p:q')
check 'global print matching lines' "$(printf 'int a;\nint c;')" "$out"

printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':g!/int/p:q')
check 'inverted global print' 'void b;' "$out"

printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':g/int/s/int/uint/\:p:q!')
check 'global with chained sub+print' "$(printf 'uint a;\nuint c;')" "$out"

printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':g/int/g/a/p:q')
check 'nested global' 'int a;' "$out"

# nested global running a substitute on the doubly-matched line only
printf 'int a;\nint b;\nvoid a;\n' > "$TMPFILE"
out=$(run_ex ':g/int/g/a/s/a/X/:%p:q!')
check 'nested global with substitute' "$(printf 'int X;\nint b;\nvoid a;')" "$out"

# triple nested global: only the line matching int AND a AND x is substituted
printf 'int a x;\nint a y;\nint b x;\nvoid a x;\n' > "$TMPFILE"
out=$(run_ex ':g/int/g/a/g/x/s/x/Z/:%p:q!')
check 'triple nested global with substitute' \
	"$(printf 'int a Z;\nint a y;\nint b x;\nvoid a x;')" "$out"

# --- escape handling: all governed by the ex_parity macro in ex.c ---
# A run of k escapes is only touched when it sits immediately before a
# delimiter; there it emits ceil(k/2) escapes (n -= n/2), and an odd k spends
# its trailing escape to make the delimiter literal (the keep&1 branch).
# ex_parity runs once per read pass, at two sites: ex_sread (the reader behind
# ex_re_read) for the regex/replacement fields, where the delimiter is whatever
# char opened the field, and ex_arg for the ex specials xsep xexp xexe
# (': % !'). A global re-executes its body, so each nesting level is one more
# body pass -> escapes before a separator double per level.

# odd run of 1 before the separator -> ':' kept literal, so :p stays in the
# global body (it is chained, not run on the whole buffer)
printf 'a/b\nc/d\nx y\n' > "$TMPFILE"
out=$(run_ex ':g/\//s/\//-/g\:p:q!')
check 'parity: \: keeps separator literal, chains print in global' \
	"$(printf 'a-b\nc-d')" "$out"

# odd run of 1 before the sub delimiter '/' in the pattern -> literal '/'
printf 'a/b\nc/d\nx y\n' > "$TMPFILE"
out=$(run_ex ':g/\//s/\//-/g:%p:q!')
check 'parity: \/ in pattern keeps delimiter literal' \
	"$(printf 'a-b\nc-d\nx y')" "$out"

# same odd-run delimiter protection, this time in the replacement field
printf 'foo\nbar\n' > "$TMPFILE"
out=$(run_ex ':g/foo/s/foo/a\/b/:%p:q!')
check 'parity: \/ in replacement keeps delimiter literal' \
	"$(printf 'a/b\nbar')" "$out"

# one body pass: \: (run 1, odd) before separator -> literal ':'
printf 'key val\n' > "$TMPFILE"
out=$(run_ex ':s/ /\: /:%p:q!')
check 'parity: 1 pass, \: -> literal colon' 'key: val' "$out"

# two body passes (global): \\\: (run 3) -> outer halves to \: -> inner to ':'
printf 'key val\nx\n' > "$TMPFILE"
out=$(run_ex ':g/key/s/ /\\\: /:%p:q!')
check 'parity: 2 passes, \\\: -> \: -> literal colon' \
	"$(printf 'key: val\nx')" "$out"

# three body passes (double global): \\\\\\\: (run 7) -> \\\: -> \: -> ':'
printf 'key val\n' > "$TMPFILE"
out=$(run_ex ':g/key/g/val/s/ /\\\\\\\: /:%p:q!')
check 'parity: 3 passes, run-7 colon halves 7->3->1' 'key: val' "$out"

# the sub delimiter '/' is consumed by ex_re_read, not the separator passes, so
# a literal '/' needs only its own single escape no matter how deep the global
printf 'foo\n' > "$TMPFILE"
out=$(run_ex ':g/foo/g/foo/s/foo/a\/b/:%p:q!')
check 'parity: sub delimiter unaffected by separator depth (1 backslash)' \
	'a/b' "$out"

# the selector /.../ is read by ex_re_read during the outer parse, so its '\:'
# is one pass (run 1 -> literal colon); the substitute lives in the body that
# the global re-executes, so its '\\\:' takes two passes (run 3 -> \: -> ':')
printf 'a:b\nc d\n' > "$TMPFILE"
out=$(run_ex ':g/\:/s/\\\:/=/:%p:q!')
check 'parity: selector colon (1 pass) vs body sub colon (2 passes)' \
	"$(printf 'a=b\nc d')" "$out"

printf 'test int here\n' > "$TMPFILE"
out=$(run_ex ':f>int:??p found:q')
check 'conditional then on search found' 'found' "$out"

printf 'test int here\n' > "$TMPFILE"
out=$(run_ex ':f>void:??!p not found:q')
check 'conditional else on search not found' 'not found' "$out"

printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ':4? 1d:%p:q!')
check 'while loop counted delete' 'e' "$out"

printf 'old\nold\n' > "$TMPFILE"
out=$(run_ex ':10? s/old/new/:%p:q!')
check 'while loop breaks on xuerr (sub no-match)' "$(printf 'new\nold')" "$out"

printf 'b\na\nc\n' > "$TMPFILE"
out=$(run_ex ':%!sort:%p:q!')
check 'sort buffer via !' "$(printf 'a\nb\nc')" "$out"

printf '' > "$TMPFILE"
out=$(run_ex ':led 0:r \!printf hello:led:%p:q!')
check 'read from pipe into empty buffer' 'hello' "$out"

# ic defaults to 1 (case-insensitive); :ic toggles it to 0 (case-sensitive)
printf 'Hello\nhello\n' > "$TMPFILE"
out=$(run_ex ':g/hello/p:q')
check 'default ic=1: case-insensitive match' "$(printf 'Hello\nhello')" "$out"

printf 'Hello\nhello\n' > "$TMPFILE"
out=$(run_ex ':ic:g/hello/p:ic:q')
check ':ic toggles to case-sensitive match' 'hello' "$out"

printf 'test\n' > "$TMPFILE"
run_ex ':q' >/dev/null 2>&1; rc=$?
check_exit 'exit code :q -> 0' '0' "$rc"

printf 'test\n' > "$TMPFILE"
run_ex ':q 3' >/dev/null 2>&1; rc=$?
check_exit 'exit code :q 3 -> 3' '3' "$rc"

printf 'test\n' > "$TMPFILE"
out=$(run_ex ':f>nosuch:p ok:q')
check 'xuerr is silent and chain continues' 'ok' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_vi 'dw')
check 'vi dw: delete word' 'world' "$out"

printf 'line1\nline2\nline3\n' > "$TMPFILE"
out=$(run_vi 'dd')
check 'vi dd: delete line' "$(printf 'line2\nline3')" "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi 'x')
check 'vi x: delete char' 'ello' "$out"

printf 'line1\nline2\n' > "$TMPFILE"
out=$(run_vi 'J')
check 'vi J: join lines with space' 'line1 line2' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_vi 'gUw')
check 'vi gUw: uppercase word' 'HELLO world' "$out"

printf 'HELLO world\n' > "$TMPFILE"
out=$(run_vi 'guw')
check 'vi guw: lowercase word' 'hello world' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi '~')
check 'vi ~: toggle case of char' 'Hello' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi 'rH')
check 'vi rH: replace char' 'Hello' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi 'yyp')
check 'vi yyp: yank and paste line' "$(printf 'hello\nhello')" "$out"

printf 'world\n' > "$TMPFILE"
out=$(run_vi "$(printf 'ihello \033')")
check 'vi i...<ESC>: insert at cursor' 'hello world' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi "$(printf 'A world\033')")
check 'vi A...<ESC>: append at end of line' 'hello world' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':%s/hello/world/:ud:%p:q!')
check 'ex :ud undoes substitute' 'hello' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':%s/hello/world/:ud:rd:%p:q!')
check 'ex :rd redoes after undo' 'world' "$out"

printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ":2m 97:4m 98:'97,'98p:q")
check 'marks: print range via marks' "$(printf 'b\nc\nd')" "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':;6p:q')
check 'horizontal range ;6 prints from offset 6' 'world' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':;0;5p:q')
check 'horizontal range ;0;5 prints chars 0-4' 'hello' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':f>hello:%s//world/:%p:q!')
check 'empty pattern reuses last keyword' 'world world' "$out"

printf 'cat and dog\n' > "$TMPFILE"
out=$(run_ex ':%s/cat|dog/pet/g:%p:q!')
check 'regex alternation in substitute' 'pet and pet' "$out"

printf 'aaa\nbb\nccc\n' > "$TMPFILE"
out=$(run_ex ':%g/b{2}/p:q')
check 'regex quantifier {2} matches' 'bb' "$out"

printf 'foobar\n' > "$TMPFILE"
out=$(run_ex ':%s/foo(?=bar)/FOO/:%p:q!')
check 'regex lookahead in substitute' 'FOObar' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':%s/(hello)/[\0]/:%p:q!')
check 'substitute backreference \\0 full match' '[hello] world' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':%s/(he)(llo)/[\1]/:%p:q!')
check 'substitute backreference \\1 group 1' '[he] world' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':%s/hello //:%p:q!')
check 'substitute with empty replacement deletes match' 'world' "$out"

printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':1,2f>void:.p:q')
check 'ranged :f> search then print current line' 'void b;' "$out"

printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ':2,3g/./p:q')
check 'global command with explicit range' "$(printf 'b\nc')" "$out"

printf 'test\n' > "$TMPFILE"
run_ex ':s/test/done/:bs:q' >/dev/null 2>&1; rc=$?
check_exit ':bs marks buffer saved so :q exits 0' '0' "$rc"

printf 'first\nsecond\nthird\n' > "$TMPFILE"
out=$(run_ex ':$-1p:q')
check 'range arithmetic $-1 prints second-to-last' 'second' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':%!tr a-z A-Z:%p:q!')
check 'pipe through tr a-z A-Z uppercases' 'HELLO WORLD' "$out"

printf 'test\n' > "$TMPFILE"
out=$(run_ex ':97reg hello:p %@97:q')
check 'register expansion %@97 in :p' 'hello' "$out"

# % expands to the current buffer path; %@<#> to a register; %<#> to a buffer;
# %# to the previous buffer. A backslash escapes the following %, and separates a
# path expansion from trailing literal digits (so they are not read as a buffer #).

printf 'test\n' > "$TMPFILE"

# %\<digits>: path expansion, then the \ separates literal trailing digits
out=$(run_ex ':p %\123:q')
check 'C12b %\123 — path then literal digits' "${TMPFILE}123" "$out"

# %\#: # is not a digit, so the \ stays literal and # is not consumed as %#
out=$(run_ex ':p %\#:q')
check 'C12b %\# — path then literal \#' "${TMPFILE}\\#" "$out"

# \%: escaped %, emitted literally with no expansion
out=$(run_ex ':p \%123:q')
check 'C12b \%123 — escaped %, fully literal' '%123' "$out"

# %0: explicit buffer 0 (the current file); \ separates trailing digits
out=$(run_ex ':p %0\132:q')
check 'C12b %0\132 — buffer 0 path then literal digits' "${TMPFILE}132" "$out"

# %@ with a non-digit: not a register ref; emits path plus a literal @
out=$(run_ex ':p %@asd:q')
check 'C12b %@asd — non-digit after %@ emits path + literal @' "${TMPFILE}@asd" "$out"

# %\": \ before " stays literal (only %, :, ! are escapable delimiters here)
out=$(run_ex ':p %\"324:q')
check 'C12b %\"324 — backslash before quote stays literal' "${TMPFILE}\\\"324" "$out"

# %@<#> register ref; \ separates the register number from trailing literal digits
out=$(run_ex ':100reg REGVAL:p %@100\75:q')
check 'C12b %@100\75 — register 100 then literal digits' 'REGVAL75' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':err 3:s/void/x/:p after:q')
check 'err=3 breaks chain on xuerr' '' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':err 4:s/void/x/:p after:q')
check 'err=4 silences error and chain continues' 'after' "$out"

printf 'test\n' > "$TMPFILE"
run_ex ':wq! /dev/null' >/dev/null 2>&1; rc=$?
check_exit ':wq! exits 0' '0' "$rc"

printf 'abc\nxyz\nabc\n' > "$TMPFILE"
out=$(run_ex ':3f<abc:p:q')
check ':f< searches backward' 'abc' "$out"

printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':g!/int/s/void/VOID/\:p:q!')
check 'g! inverted global chained sub+print' 'VOID b;' "$out"

printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ':,2+3=1:q')
check 'range arithmetic ,2+3 prints 5' '5' "$out"

printf 'line1\nline2\nline3\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:3ya 98:1pu 98:1pu 97:%p:q!')
check 'yank two registers and paste both' \
	"$(printf 'line1\nline1\nline3\nline2\nline3')" "$out"

printf 'old new\n' > "$TMPFILE"
out=$(run_vi "$(printf 'cwfresh\033')")
check 'vi cw: change word' 'freshnew' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi "$(printf 'Cworld\033')")
check 'vi C: change to end of line' 'world' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_vi "$(printf '5lD')")
check 'vi D: delete to end of line' 'hello' "$out"

printf 'line1\n' > "$TMPFILE"
out=$(run_vi "$(printf 'onewline\033')")
check 'vi o: open line below' "$(printf 'line1\nnewline')" "$out"

printf 'line2\n' > "$TMPFILE"
out=$(run_vi "$(printf 'Onewline\033')")
check 'vi O: open line above' "$(printf 'newline\nline2')" "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi "$(printf 'sH\033')")
check 'vi s: substitute char' 'Hello' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_vi "$(printf 'Snew\033')")
check 'vi S: substitute line' 'new' "$out"

printf 'line1\nline2\n' > "$TMPFILE"
out=$(run_vi 'yyjP')
check 'vi P: paste above current line' "$(printf 'line1\nline1\nline2')" "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi 'xu')
check 'vi u: undo restores deleted char' 'hello' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_vi '>>')
check 'vi >>: indent adds tab' "$(printf '\thello')" "$out"

printf 'foo(bar)qux\n' > "$TMPFILE"
out=$(run_vi 'di(')
check 'vi di(: delete inside parens' 'foo()qux' "$out"

printf 'foo(bar)qux\n' > "$TMPFILE"
out=$(run_vi "$(printf 'ci(new\033')")
check 'vi ci(: change inside parens' 'foo(new)qux' "$out"

printf 'one two three\n' > "$TMPFILE"
out=$(run_vi '2dw')
check 'vi 2dw: delete 2 words' 'three' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_vi 'fwx')
check 'vi fw+x: find char then delete it' 'hello orld' "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_vi "$(printf '5lK')")
check 'vi K: split line at cursor' "$(printf 'hello \nworld')" "$out"

# E1: :& takes raw vi input; to use a register, expand it via %@97
printf 'hello world\n' > "$TMPFILE"
check 'E1 :& %@97 — register expansion used as raw vi input' 'world' \
	"$(run_mac ":97reg dw:& %@97:w! $OUTFILE:q!")"

# E2: \:cmd inside & macro; a newline (0x0A) is required to submit the ex cmd
printf 'hello\n' > "$TMPFILE"
check 'E2 :& \:s// — ex cmd from macro (newline submits)' 'world' \
	"$(run_mac "$(printf ':& \\:s/hello/world/\n:w! %s:q' "$OUTFILE")")"

# E3: vi normal &a — executes register 'a' as a non-blocking macro
printf 'hello world\n' > "$TMPFILE"
check 'E3 vi &a — executes register as non-blocking macro' 'world' \
	"$(run_mac ":97reg dw:& &a:w! $OUTFILE:q!")"

# E4: vi normal && — repeats the last & macro
printf 'hello world foo\n' > "$TMPFILE"
check 'E4 vi && — repeats last & macro' 'foo' \
	"$(run_mac ":97reg dw:& &a&&:w! $OUTFILE:q!")"

# ya! 97 frees register 97; pu 97 on a freed register raises "uninitialized
# register" — with err 4 (silence+ignore) the paste is skipped silently.
printf 'line1\nline2\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:ya! 97:err 4:$pu 97:%p:q!')
check 'G1 ya! frees reg; pu 97 silently skipped (err 4)' \
	"$(printf 'line1\nline2')" "$out"

# A prefix with no argument captures the current error status into that id;
# the same prefix with an argument branches on the captured value (??! inverts,
# both when capturing and when branching). An intervening command that changes
# the error status does NOT override the captured value.
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':f>void:5??:s/hello/world/:5??!p notfound:q!')
check 'H1 ?? id captures fail; intervening success does not override' 'notfound' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':f>hello:5??:f>void:5??p found:q')
check 'H2 ?? id captures success; intervening fail does not override' 'found' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':f>void:??!p not found:q')
check 'I1 ??! — fires branch on failure' 'not found' "$out"

printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':f>hello:??p found:q')
check 'I2 ?? — fires branch on success' 'found' "$out"

# ?! runs the body while the condition fails; 1d modifies buffer state and
# persists across iterations until 'yes' surfaces to line 1.
printf 'no\nno\nyes\nno\n' > "$TMPFILE"
out=$(run_ex ':10?! f>yes\:1??\!\:1??1d\:1???\:??\!:.p:q!')
check 'I3 ?! while: 1d deletes first line each pass until f>yes succeeds' 'yes' "$out"

# seq 0 groups all subsequent changes into a single undo step
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':seq 0:s/hello/step1/:s/step1/step2/:s/step2/final/:seq:ud:%p:q!')
check 'J1 seq 0 — batch changes undo as one step' 'hello' "$out"

# seq -1 disables undo tracking entirely; :ud has no effect
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':seq -1:s/hello/world/:ud:%p:seq:q!')
check 'J2 seq -1 — undo tracking disabled; u has no effect' 'world' "$out"

# pr N redirects :p output to register N; led 0 suppresses double-printing
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':led 0:pr 97:p hello captured:pr 0:led:pu 97:%p:q!')
check 'K1 pr+led 0 — :p output captured into register, then pasted' \
	"$(printf 'hello\nhello captured')" "$out"

printf 'a\nb\nc\nd\n' > "$TMPFILE"
out=$(run_ex ":2,3s/./X/:'91p:q!")
check "L1 '[ marks first changed line" 'X' "$out"

printf 'a\nb\nc\nd\n' > "$TMPFILE"
out=$(run_ex ":2,3s/./X/:'93p:q!")
check "L2 '] marks last changed line" 'X' "$out"

# '* = cursor position saved BEFORE the previous ex command ran
printf 'a\nb\nc\nd\n' > "$TMPFILE"
out=$(run_ex ":3p:'42p:q")
check "L3 '* = cursor saved before previous ex command" \
	"$(printf 'c\na')" "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':f>hello:p %@47:q')
check 'M1 %@47 expands to the previous regex keyword' 'hello' "$out"

# $*5/10 — navigate to 50% of the file (integer arithmetic on last line)
printf 'a\nb\nc\nd\ne\nf\ng\nh\ni\nj\n' > "$TMPFILE"
out=$(run_ex ':$*5/10p:q')
check 'N1 $*5/10 — navigate to 50% of file' 'e' "$out"

# ;5;#+10 — # rebases to the previous semicolon value (5), so +10 = 15;
# = 3 prints the second char offset (the final computed value).
printf 'hello world extra\n' > "$TMPFILE"
out=$(run_ex ':;5;#+10= 3:q')
check 'N2 ;5;#+10= 3 — second char offset via # rebase is 15' '15' "$out"

# f( lands on (; \% passes a literal % (not buffer path) to the & macro
printf 'foo(bar)qux\n' > "$TMPFILE"
out=$(run_vi 'f(\%x')
check 'O1 vi f(\%x — find (, jump to matching ), delete )' 'foo(barqux' "$out"

rm -f "$OUTFILE"
printf 'test\n' > "$TMPFILE"
run_ex ":97reg hello world:pu 97 \!tr a-z A-Z > $OUTFILE:q" >/dev/null 2>/dev/null
check 'Y1 :pu 97 \!cmd — pipe register content to external command' \
	'HELLO WORLD' "$(cat $OUTFILE 2>/dev/null)"

rm -f "$OUTFILE"
printf 'hello world\n' > "$TMPFILE"
run_ex ":1,1w \!tr a-z A-Z > $OUTFILE:q" >/dev/null 2>/dev/null
check 'Y2 :w \!cmd — write buffer range to external command' \
	'HELLO WORLD' "$(tr -d '\n' < $OUTFILE 2>/dev/null)"

# 1q inside a nested ??! branch propagates xquit through 2 ex_exec levels but
# must restore it at the outermost so vi keeps running. Without the base case
# in ex_exec, xquit=6 escapes to vi and produces exit code 5.
printf 'hello\n' > "$TMPFILE"
EXINIT=":f>void:??!1q 5:q" "$VI" -sm "$TMPFILE" </dev/null >/dev/null 2>&1; rc=$?
check_exit 'Q1 1q in nested ??! scope does not propagate quit to vi' '0' "$rc"

# R1: :re word sets the keyword; %@47 reflects it
printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':re world:p %@47:q')
check 'R1 :re word — sets keyword; %@47 returns it' 'world' "$out"

# R2: :re word sets the keyword; :g// (empty pattern) reuses it
printf 'hello\nworld\nhello world\n' > "$TMPFILE"
out=$(run_ex ':re hello:g//p:q')
check 'R2 :re word; :g// uses set keyword to match lines' \
	"$(printf 'hello\nhello world')" "$out"

# R3: :re sets keyword without moving the cursor (unlike :f>)
printf 'foo\nbar\nbaz\n' > "$TMPFILE"
out=$(run_ex ':3:re bar:.p:q')
check 'R3 :re does not navigate; cursor stays on current line' 'baz' "$out"

# R4: range form :1re escapes regex-special chars; verify via %@47
# Buffer line 1 is "a.b"; ex_regesc turns "." into "\.".
# (The trailing \n from lbuf_region is included so %@47 output is "a\.b"
# after command-substitution strips the trailing newline.)
printf 'a.b\naXb\n' > "$TMPFILE"
out=$(run_ex ':1re:p %@47:q')
check 'R4 range :re — escapes regex chars; %@47 reflects escaped pattern' 'a\.b' "$out"

printf 'void a;\nint b;\nvoid c;\n' > "$TMPFILE"
out=$(run_ex ':>int>p:q')
check 'S1 >int>p — forward inline-search range address prints found line' 'int b;' "$out"

printf 'void a;\nint b;\nvoid c;\n' > "$TMPFILE"
out=$(run_ex ':3:<int<p:q')
check 'S2 3:<int<p — backward inline-search range address' 'int b;' "$out"

printf 'void a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':.,>int>p:q')
check 'S3 .,>int>p — range from current line to forward match' \
	"$(printf 'void a;\nvoid b;\nint c;')" "$out"

printf 'a\nb\nc\nd\ne\nf\ng\n' > "$TMPFILE"
out=$(run_ex ':4:.-2,.+2p:q')
check 'S4 .-2,.+2p — relative arithmetic prints 5 lines around line 4' \
	"$(printf 'b\nc\nd\ne\nf')" "$out"

printf 'hello world\n' > "$TMPFILE"
out=$(run_ex ':;5= 2:q')
check 'T1 ;5= 2 — print character offset 5' '5' "$out"

printf 'a\nb\nc\nd\n' > "$TMPFILE"
out=$(run_ex ":3m 97:'97= 0:q")
check "T2 '97= 0 — print stored row index of mark a" '2' "$out"

printf 'hello world extra padding here\n' > "$TMPFILE"
out=$(run_ex ':;5;+10= 3:q')
check 'T3 ;5;+10= 3 — second offset from initial (0+10=10), not from 5' '10' "$out"

# U1: g/^$/d — remove all blank lines
printf 'a\n\nb\n\nc\n' > "$TMPFILE"
out=$(run_ex ':g/^$/d:%p:q!')
check 'U1 g/^$/d — removes all blank lines' "$(printf 'a\nb\nc')" "$out"

# U2: g/int/g/;$/& — nested global appends text; & requires -e mode
printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_mac ':g/int/g/;$/& A has a semicolon:w! '"$OUTFILE"':q')
check 'U2 g/int/g/;$/& A has a semicolon — nested global appends text' \
	"$(printf 'int a; has a semicolon\nvoid b;\nint c; has a semicolon')" "$out"

# U3: err 1 — print errors but continue chain
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:s/void/x/:p after:q')
check 'U3 err=1 — prints error; chain continues' 'after' "$out"

# U4: 2??.= — unset id tag; branch does not execute
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':2??.=:p reached:q')
check 'U4 2??.= — unset id; branch does not run; chain continues' 'reached' "$out"

# U5: grp 1:%f+int(.):grp — the search lands on group 1, not on the whole
# match, so the cursor stops on the character right after "int" (the "(" at
# offset 8) instead of on the "i" at offset 5. "(.)" is the capture group here,
# not a literal paren.
printf 'void int(x)\n' > "$TMPFILE"
out=$(run_ex ':grp 1:%f+int(.):grp:;= 2:q')
check 'U5 grp 1:%f+int(.):grp — char offset after "int" is 8' '8' "$out"

# U6: ;5;#+10= 3 — #+10 is relative to previous offset 5 (not initial)
printf 'hello world extra padding here\n' > "$TMPFILE"
out=$(run_ex ':;5;#+10= 3:q')
check 'U6 ;5;#+10= 3 — #+10 relative to 5; offset = 15' '15' "$out"

# U7: 2,4f>int — ranged :f> restricts search to lines 2-4
printf 'a\nb\nint c;\nd\nint e;\nf\n' > "$TMPFILE"
out=$(run_ex ':2,4f>int:.p:q')
check 'U7 2,4f>int — ranged :f> restricts search to given line range' 'int c;' "$out"

# U8: :%f>marker:??!p no marker\:1q:%s/old/new/g:w
# (U8a removed — Q1 covers the abort case)
# When marker found: ??! else branch skipped; :s and :w execute
printf 'marker here\nold text\n' > "$TMPFILE"
EXINIT=":%f>marker:??!p no marker\:1q:%s/old/new/g:w! $OUTFILE" \
	"$VI" -sm "$TMPFILE" </dev/null >/dev/null 2>&1
check 'U8b marker found — else skipped; :s and :w execute' \
	"$(printf 'marker here\nnew text')" "$(cat $OUTFILE 2>/dev/null)"

# U9: :%!sort — pipe buffer through external command
printf 'c\na\nb\n' > "$TMPFILE"
out=$(run_ex ':%!sort:%p:q!')
check 'U9 :%!sort — pipe buffer through sort; output replaces buffer' \
	"$(printf 'a\nb\nc')" "$out"

# U10: g/int/ya+ 97 — global appends matching lines to register 97
printf 'int a;\nvoid b;\nint c;\n' > "$TMPFILE"
out=$(run_ex ':led 0:g/int/ya+ 97:led:1pu 97:%p:q!')
check 'U10 g/int/ya+ 97 — global appends matching lines to register 97' \
	"$(printf 'int a;\nint a;\nint c;\nvoid b;\nint c;')" "$out"

# U11: \0 is the whole match, not a group; only the first match is substituted
# because there is no g flag, so "void" is left alone
printf 'this has int or void\n' > "$TMPFILE"
out=$(run_ex ':%s/(int)|(void)/pre\0after:%p:q!')
check 'U11 :%s/(int)|(void)/pre\0after — \0 is the whole match' \
	'this has preintafter or void' "$out"

# U12: 3,5r \!printf — range selects lines 3-5 from pipe; inserts before current line
printf 'start\n' > "$TMPFILE"
out=$(EXINIT=':led 0:3,5r \!printf '"'"'a\nb\nc\nd\ne\nf\n'"'"':led:%p:q!' \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'U12 3,5r \!printf — read only lines 3-5 from pipe output' \
	"$(printf 'c\nd\ne\nstart')" "$out"

# U13: ;$+1!echo world — insert cmd output after end-of-line
printf 'hello\n' > "$TMPFILE"
out=$(EXINIT=':;$+1!echo world:%p:q!' \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'U13 ;$+1!echo world — cmd output inserted at end; original preserved' \
	"$(printf 'hello\nworld')" "$out"

# V1: AND — both succeed → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:f>hello:2??:1,2??p then:q')
check 'V1 1,2?? AND — both succeed → then' 'then' "$out"

# V2: AND — first fails → else
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>hello:2??:1,2??!p else:q')
check 'V2 1,2?? AND — first fails → else' 'else' "$out"

# V3: AND — second fails → else
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:f>nomatch:2??:1,2??!p else:q')
check 'V3 1,2?? AND — second fails → else' 'else' "$out"

# V4: OR — first succeeds → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:f>nomatch:2??:1;2??p then:q')
check 'V4 1;2?? OR — first succeeds → then' 'then' "$out"

# V5: OR — first fails, second succeeds → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>hello:2??:1;2??p then:q')
check 'V5 1;2?? OR — second succeeds → then' 'then' "$out"

# V6: OR — both fail → else
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>nomatch:2??:1;2??!p else:q')
check 'V6 1;2?? OR — both fail → else' 'else' "$out"

# V7: mixed (1,2;3) — AND group fails, id3 succeeds → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>hello:2??:f>hello:3??:1,2;3??p then:q')
check 'V7 1,2;3?? — AND group fails, id3 succeeds → then' 'then' "$out"

# V8: mixed (1,2;3) — AND group succeeds, id3 fails → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:f>hello:2??:f>nomatch:3??:1,2;3??p then:q')
check 'V8 1,2;3?? — AND group succeeds, id3 fails → then' 'then' "$out"

# V9: mixed (1,2;3) — all fail → else
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>nomatch:2??:f>nomatch:3??:1,2;3??!p else:q')
check 'V9 1,2;3?? — all fail → else' 'else' "$out"

# V10: unset id in AND → nop, chain continues
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:1,99??p then:p reached:q')
check 'V10 1,99?? — id 99 unset → nop, chain continues' 'reached' "$out"

# V11: unset id in OR → nop even if other id is set
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:1;99??p then:p reached:q')
check 'V11 1;99?? — id 99 unset → nop, chain continues' 'reached' "$out"

# V12: single unset id → nop
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:99??p then:p reached:q')
check 'V12 99?? — single unset id → nop, chain continues' 'reached' "$out"

# V13: unset id in AND within complex prefix (1,99;2) → nop
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:f>hello:2??:1,99;2??p then:p reached:q')
check 'V13 1,99;2?? — unset id in AND position → nop' 'reached' "$out"

# V14: unset id in first OR position (99;1) → nop
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:99;1??p then:p reached:q')
check 'V14 99;1?? — unset id in first OR position → nop' 'reached' "$out"

# V15: unset id in middle OR position (1;99;2) → nop
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:f>hello:2??:1;99;2??p then:p reached:q')
check 'V15 1;99;2?? — unset id in middle OR position → nop' 'reached' "$out"

# V16-V19: interleaved 1,2;3,4,5;6 = (1 AND 2) OR (3 AND 4 AND 5) OR 6
# V16: first AND group succeeds → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??:f>hello:2??:f>nomatch:3??:f>nomatch:4??:f>nomatch:5??:f>nomatch:6??:1,2;3,4,5;6??p then:q')
check 'V16 1,2;3,4,5;6?? — first group (1 AND 2) succeeds → then' 'then' "$out"

# V17: only middle AND group succeeds → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>hello:2??:f>hello:3??:f>hello:4??:f>hello:5??:f>nomatch:6??:1,2;3,4,5;6??p then:q')
check 'V17 1,2;3,4,5;6?? — middle group (3 AND 4 AND 5) succeeds → then' 'then' "$out"

# V18: only last OR operand succeeds → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>nomatch:2??:f>nomatch:3??:f>hello:4??:f>hello:5??:f>hello:6??:1,2;3,4,5;6??p then:q')
check 'V18 1,2;3,4,5;6?? — last operand (6) succeeds → then' 'then' "$out"

# V19: all groups fail → else
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??:f>nomatch:2??:f>nomatch:3??:f>nomatch:4??:f>nomatch:5??:f>nomatch:6??:1,2;3,4,5;6??!p else:q')
check 'V19 1,2;3,4,5;6?? — all groups fail → else' 'else' "$out"

# W1: command succeeds, ??! captures as failure → else
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??!:1??!p else:q')
check 'W1 ??! — success captured as failure → else' 'else' "$out"

# W2: command fails, ??! captures as success → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??!:1??p then:q')
check 'W2 ??! — failure captured as success → then' 'then' "$out"

# W3: inverted capture used in AND with normal capture
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nomatch:1??!:f>hello:2??:1,2??p then:q')
check 'W3 ??! in AND — NOT(fail) AND success → then' 'then' "$out"

# W4: inverted capture in OR — NOT(success) OR success → then
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??!:f>hello:2??:1;2??p then:q')
check 'W4 ??! in OR — NOT(success) OR success → then' 'then' "$out"

# W5: both inverted — NOT(success) AND NOT(success) → else
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:1??!:f>hello:2??!:1,2??!p else:q')
check 'W5 ??! both inverted — NOT(success) AND NOT(success) → else' 'else' "$out"

# W6: invert the status of last command
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>hello:??!:??p hidden:f>nope:??!:??p show:q')
check 'W6 ??! inverted the status of last command' 'show' "$out"

# W7: pass the status of last command
printf 'hello\n' > "$TMPFILE"
out=$(run_ex ':err 1:f>nope:??:??!p hidden:q')
check 'W7 ?? pass the status of last command' 'hidden' "$out"

# M1/M2: pure deletion BEFORE mark — mark shifts down, undo restores
printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ":4m 97:1,2d:'97=1:q")
check 'mark: pure delete before → mark shifts' '2' "$out"
out=$(run_ex ":4m 97:1,2d:ud:'97=1:q")
check 'mark: undo pure delete before → mark restored' '4' "$out"

# M3/M4: pure deletion AFTER mark — mark unchanged, undo leaves mark unchanged
printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ":2m 97:4,5d:'97=1:q")
check 'mark: pure delete after → mark unchanged' '2' "$out"
out=$(run_ex ":2m 97:4,5d:ud:'97=1:q")
check 'mark: undo pure delete after → mark unchanged' '2' "$out"

# M5: pure deletion AT mark — mark invalidated, undo restores it
printf 'a\nb\nc\nd\ne\n' > "$TMPFILE"
out=$(run_ex ":3m 97:3d:ud:'97=1:q")
check 'mark: undo pure delete at mark → mark restored' '3' "$out"

# M6/M7: replacement COVERING mark (n_ins>0, n_del>0) — mark clamped, undo restores
# :1,3;0;d joins lines 1-3 into one replacement line; mark at line 3 is clamped to 1
printf 'aa\nbb\ncc\ndd\nee\n' > "$TMPFILE"
out=$(run_ex ":3m 97:1,3;0;d:'97=1:q")
check 'mark: replacement covers mark → mark clamped' '1' "$out"
out=$(run_ex ":3m 97:1,3;0;d:ud:'97=1:q")
check 'mark: undo replacement covering mark → mark restored' '3' "$out"

# M8/M9: replacement BEFORE mark — mark adjusts, undo restores
# :1,2;0;d replaces lines 1-2 with one line; mark at line 4 shifts to 3
printf 'aa\nbb\ncc\ndd\n' > "$TMPFILE"
out=$(run_ex ":4m 97:1,2;0;d:'97=1:q")
check 'mark: replacement before → mark adjusts' '3' "$out"
out=$(run_ex ":4m 97:1,2;0;d:ud:'97=1:q")
check 'mark: undo replacement before → mark restored' '4' "$out"

# M10/M11: replacement AFTER mark — mark unchanged, undo leaves mark unchanged
printf 'aa\nbb\ncc\ndd\n' > "$TMPFILE"
out=$(run_ex ":1m 97:3,4;0;d:'97=1:q")
check 'mark: replacement after → mark unchanged' '1' "$out"
out=$(run_ex ":1m 97:3,4;0;d:ud:'97=1:q")
check 'mark: undo replacement after → mark unchanged' '1' "$out"

# For :c the replacement lines are embedded directly in the EXINIT string.
# In raw ex mode (-sm, xvis & 1) a non-empty argument is injected straight into
# the insertion buffer and bypasses the interactive reader entirely, so there is
# no "." terminator line to write.
# printf converts \n in its format to real newlines; ex_arg stops at ':' (xsep)
# so newlines inside the arg are part of the text, not command separators.
# All tests use a 5-line file: aa bb cc dd ee.

# MC1/MC2: n_ins=2, n_del=3 — :2,4c replaces lines 2-4 with "xx","yy"
# mark at line 4 (row 3) is in lossy zone [pos+n_ins, pos+n_del) = [3,4)
# forward: clamped to pos+n_ins-1 = 2 = line 3; undo: restored to line 4
printf 'aa\nbb\ncc\ndd\nee\n' > "$TMPFILE"
out=$(EXINIT="$(printf ':4m 97:2,4c xx\nyy:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: :c lossy zone (n_ins=2,n_del=3) → mark clamped' '3' "$out"
out=$(EXINIT="$(printf ':4m 97:2,4c xx\nyy:ud:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: undo :c lossy zone → mark restored' '4' "$out"

# MC3/MC4: mark inside changed region but before lossy zone (row 2 < pos+n_ins=3)
# not saved; stays at row 2 = line 3 both forward and after undo
printf 'aa\nbb\ncc\ndd\nee\n' > "$TMPFILE"
out=$(EXINIT="$(printf ':3m 97:2,4c xx\nyy:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: :c mark before lossy zone → mark unchanged' '3' "$out"
out=$(EXINIT="$(printf ':3m 97:2,4c xx\nyy:ud:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: undo :c; mark was before lossy zone → still unchanged' '3' "$out"

# MC5/MC6: n_ins=1, n_del=3 — :2,4c with 1 line; mark at row 2 falls inside
# lossy zone [pos+n_ins, pos+n_del) = [2,4) → clamped to pos+n_ins-1=1 = line 2;
# undo restores to line 3
printf 'aa\nbb\ncc\ndd\nee\n' > "$TMPFILE"
out=$(EXINIT="$(printf ':3m 97:2,4c xx:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: :c fully steps over mark (n_ins=1,n_del=3) → mark clamped' '2' "$out"
out=$(EXINIT="$(printf ':3m 97:2,4c xx:ud:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: undo :c fully stepping over mark → mark restored' '3' "$out"

# MC7/MC8: mark after changed region (n_ins<n_del) → arithmetic −1; undo +1
printf 'aa\nbb\ncc\ndd\nee\n' > "$TMPFILE"
out=$(EXINIT="$(printf ':5m 97:2,4c xx\nyy:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: :c before mark (n_ins<n_del) → mark adjusts down' '4' "$out"
out=$(EXINIT="$(printf ':5m 97:2,4c xx\nyy:ud:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: undo :c before mark (n_ins<n_del) → mark restored' '5' "$out"

# MC9/MC10: n_ins=3, n_del=2 — mark at last deleted row (row 2 = pos+n_del-1)
# row 2 is within new insertion range [pos, pos+n_ins) = [1,4); not in empty lossy
# zone; not saved → mark stays at row 2 = line 3 forward and after undo
printf 'aa\nbb\ncc\ndd\nee\n' > "$TMPFILE"
out=$(EXINIT="$(printf ':3m 97:2,3c xx\nyy\nzz:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: :c deleted row within new range (n_ins>n_del) → not invalidated' '3' "$out"
out=$(EXINIT="$(printf ':3m 97:2,3c xx\nyy\nzz:ud:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: undo :c deleted row within new range → mark unchanged' '3' "$out"

# MC11/MC12: n_ins=3, n_del=2 — :2,3c inserts more than deleted; lossy zone empty
# mark after (row 4 = line 5) adjusts +1 → line 6; undo adjusts −1 → line 5
printf 'aa\nbb\ncc\ndd\nee\n' > "$TMPFILE"
out=$(EXINIT="$(printf ':5m 97:2,3c xx\nyy\nzz:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: :c before mark (n_ins>n_del) → mark adjusts up' '6' "$out"
out=$(EXINIT="$(printf ':5m 97:2,3c xx\nyy\nzz:ud:'"'"'97=1:q')" \
	"$VI" -sm "$TMPFILE" </dev/null 2>/dev/null)
check 'mark: undo :c before mark (n_ins>n_del) → mark restored' '5' "$out"

# ──────────────────────────────────────────────────────────────────────────────
# Substitute escape behavior — :s/pat/repl/ escapes and delimiter edge cases.
# The replacement layer drops a backslash before any char (keeping the char
# literal) EXCEPT digits 0-9 (backreferences; \0 = whole match) and \\ (one \).
# The delimiter and the ':' command separator are themselves escapable with \.
# ──────────────────────────────────────────────────────────────────────────────

printf 'a/b end\n' > "$TMPFILE"
out=$(run_ex ':%s/a\/b/X/:%p:q!')
check 'X1 \/ — escaped delimiter is literal in pattern' 'X end' "$out"

printf 'x\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\/b/:%p:q!')
check 'X2 \/ — escaped delimiter is literal in replacement' 'a/b' "$out"

printf 'x\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\\b/:%p:q!')
check 'X3 \\\\ — two backslashes become one literal backslash' 'a\b' "$out"

printf 'x\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\tb/:%p:q!')
check 'X4 \\t — backslash before ordinary char drops; no tab expansion' 'atb' "$out"

printf 'x\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\nb/:%p:q!')
check 'X5 \\n — no newline expansion in replacement; literal n' 'anb' "$out"

printf 'x\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\&b/:%p:q!')
check 'X6 \& — & is literal anyway; backslash drops' 'a&b' "$out"

printf 'ab\n' > "$TMPFILE"
out=$(run_ex ':%s/ab/&/:%p:q!')
check 'X7 & — ampersand is literal, not whole-match (nextvi uses \0)' '&' "$out"

printf 'a.b aXb a.b\n' > "$TMPFILE"
out=$(run_ex ':%s/a\.b/X/g:%p:q!')
check 'X8 \. — escaped dot matches literal dot only' 'X aXb X' "$out"

printf 'a.b axb\n' > "$TMPFILE"
out=$(run_ex ':%s/a.b/X/g:%p:q!')
check 'X9 . — unescaped dot is the any-char metachar' 'X X' "$out"

printf 'a\\b c\n' > "$TMPFILE"
out=$(run_ex ':%s/a\\b/X/:%p:q!')
check 'X10 \\\\ in pattern matches one literal backslash' 'X c' "$out"

printf 'ab\n' > "$TMPFILE"
out=$(run_ex ':%s/(a)(b)/\2\1/:%p:q!')
check 'X11 \1 \2 — backreference group swap' 'ba' "$out"

printf 'ab\n' > "$TMPFILE"
out=$(run_ex ':%s/ab/[\0]/:%p:q!')
check 'X12 \0 — backreference to the whole match' '[ab]' "$out"

printf 'x\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\1b/:%p:q!')
check 'X13 \1 with no such group — backslash drops; literal digit' 'a1b' "$out"

printf 'a/b c\n' > "$TMPFILE"
out=$(run_ex ':%s,a/b,X,:%p:q!')
check 'X14 , delimiter — / needs no escaping under a comma delimiter' 'X c' "$out"

printf 'a/b/c\n' > "$TMPFILE"
out=$(run_ex ':%s/\//_/g:%p:q!')
check 'X15 \/ g — every escaped delimiter replaced' 'a_b_c' "$out"

printf 'x/ z\n' > "$TMPFILE"
out=$(run_ex ':%s/x\/:%p:q!')
check 'X16 \/ ends pattern with no replacement section — match deleted' ' z' "$out"

# Delimiter/separator interaction: an unescaped ':' ends the s command; a
# backslash before ':' escapes the separator into the replacement text.
printf 'x z\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a:b/:%p:q!')
check 'X17 : — unescaped separator ends replacement at "a"' 'a z' "$out"

printf 'x z\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\:b/:%p:q!')
check 'X18 \: — escaped separator stays literal in replacement' 'a:b z' "$out"

printf 'x z\n' > "$TMPFILE"
out=$(run_ex ':%s/x/a\\:b/:%p:q!')
check 'X19 \\\\: — \\ collapses to one \; the bare : still separates' 'a\ z' "$out"

printf 'x z\n' > "$TMPFILE"
out=$(run_ex ':%s/x/y\\:%p:q!')
check 'X20 trailing \\ before separator — replacement ends in literal \' 'y\ z' "$out"

printf 'x z\n' > "$TMPFILE"
out=$(run_ex ':%s/x/y\:%p:q!')
check 'X21 trailing \ escapes separator — :%p swallowed; nothing printed' '' "$out"

# A non-digit escape stays literal no matter how many groups the pattern has:
# the backreference index must come from the digit value, never from the raw
# distance to '0' (which aliases chars below '0' onto real group numbers).
printf 'ab\n' > "$TMPFILE"
out=$(run_ex ':%s/(a)(b)/X\.Y/:%p:q!')
check 'X22 \. with 2 groups — non-digit escape is literal, not group 2' 'X.Y' "$out"

printf 'abcd\n' > "$TMPFILE"
out=$(run_ex ':%s/(a)(b)(c)(d)/X\,Y/:%p:q!')
check 'X23 \, with 4 groups — non-digit escape is literal, not group 4' 'X,Y' "$out"

# Backreference numbers above 9: digits are consumed greedily, but only while
# the extended number is still an existing group; leftover digits stay literal.
# A leading zero never extends, so \0 keeps meaning the whole match.
G11='(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)'

printf 'abcdefghijk\n' > "$TMPFILE"
out=$(run_ex ":%s/$G11/[\\11][\\10][\\9][\\1]/:%p:q!")
check 'X24 \11 \10 with 11 groups — two-digit backreferences' '[k][j][i][a]' "$out"

printf 'abc\n' > "$TMPFILE"
out=$(run_ex ':%s/(a)(b)(c)/[\11][\3]/:%p:q!')
check 'X25 \11 with 3 groups — stops at group 1; 1 stays literal' '[a1][c]' "$out"

printf 'abcdefghijk\n' > "$TMPFILE"
out=$(run_ex ":%s/$G11/[\\12][\\110]/:%p:q!")
check 'X26 \12 \110 with 11 groups — highest existing group wins' '[a2][k0]' "$out"

printf 'abcdefghijk\n' > "$TMPFILE"
out=$(run_ex ":%s/$G11/[\\011][\\0]/:%p:q!")
check 'X27 \011 — leading zero never extends; whole match + literal 11' \
	'[abcdefghijk11][abcdefghijk]' "$out"

printf 'abcdefghijk\n' > "$TMPFILE"
out=$(run_ex ":%s/$G11/[\\\\11][\\11]/:%p:q!")
check 'X28 \\\\11 with 11 groups — escaped backslash keeps digits literal' \
	'[\11][k]' "$out"

# ──────────────────────────────────────────────────────────────────────────────
# Search-range escape parity
# Each command leads with a forward inline-search range address, '>pat>'; the
# closing delimiter may be dropped, in which case the pattern runs to the end of
# the command. Its backslash runs sit right next to the search delimiter — the
# rarest corner of the escape-halving rule. The ':? ' forms wrap the same
# address in a one-iteration while loop, which re-reads its body and so adds a
# parse pass; the leading '?' is the while loop, not a search delimiter
# (backward search is '<pat<'), and the trailing '?' of those patterns is an
# ordinary regex metachar. On a backslash-free buffer the signal is whether the
# address parses to a real (unmatched) search → "invalid range", parses twice
# → two errors, or is fully absorbed → nothing.
# ──────────────────────────────────────────────────────────────────────────────

INVRANGE='invalid range'
INVRANGE2="$(printf 'invalid range\ninvalid range')"

printf 'irrelevant\n' > "$TMPFILE"
out=$(run_ex ':? ??p\\\:1q\:p BAD:q!')
check 'P1 ? ??p\\\:1q\:p BAD — escapes absorb :1q/:p; current line printed' \
	'irrelevant' "$out"

out=$(run_ex ':>\\>:reg:q!')
check 'P2 >\\> — \\ halves to one \; search runs, no match' "$INVRANGE" "$out"

out=$(run_ex ':>\\\>>:reg:q!')
check 'P3 >\\\>> — \> kept literal inside delimiter; search runs, no match' \
	"$INVRANGE" "$out"

out=$(run_ex ':>\\\\>:reg:q!')
check 'P4 >\\\\> — two backslashes; search runs, no match' "$INVRANGE" "$out"

out=$(run_ex ':>\\\\\>>:reg:q!')
check 'P5 >\\\\\>> — odd run keeps delimiter literal; search runs, no match' \
	"$INVRANGE" "$out"

out=$(run_ex ':>\\\\\\>:reg:q!')
check 'P6 >\\\\\\> — three backslashes; search runs, no match' "$INVRANGE" "$out"

out=$(run_ex ':? >\\\?:reg:q!')
check 'P7 ? >\\\? — 3 \ then a regex "?" in the while body; no match' \
	"$INVRANGE" "$out"

out=$(run_ex ':? >\\\\?:reg:q!')
check 'P8 ? >\\\\? — 4 \ then a regex "?" in the while body; no match' \
	"$INVRANGE" "$out"

out=$(run_ex ':? >\\\\\?:reg:q!')
check 'P9 ? >\\\\\? — 5 \ then a regex "?" in the while body; no match' \
	"$INVRANGE" "$out"

out=$(run_ex ':? >\\\\\\?:reg:q!')
check 'P10 ? >\\\\\\? — 6 \ then a regex "?" in the while body; no match' \
	"$INVRANGE" "$out"

out=$(run_ex ':? ?? 1?\:p:q!')
check 'P11 ? ?? 1?\:p — ex separator escape' \
	"irrelevant" "$out"

out=$(run_ex ':? ?? 1?\\:p:q!')
check 'P12 ? ?? 1?\\:p — ex separator escape' \
	"unknown command
irrelevant" "$out"

# --- line-0 address for edit commands / :a removal --------------------------
# A line-0 address means "above the first line". Only the absolute 0 resolves
# there; relative addresses (-1, ., +1) need an existing referent line, so on
# an empty buffer they error. Insert-family commands (i, pu) accept the line-0
# region; commands that need a real line (c, and the whole-buffer commands
# r/w/ef) reject it. :a was removed and folded into i + the line-0 address.

printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':0i TOP:%p:q!')
check ':0i inserts above first line' "$(printf 'TOP\na\nb\nc')" "$out"

printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':-1i TOP:%p:q!')
check ':-1i from line 1 inserts above first line' "$(printf 'TOP\na\nb\nc')" "$out"

# :a removed; with no address the unknown command is reported (with an address
# ex just seeks to it and ignores the rest, so use the no-address form here)
printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':a X:%p:q!')
check ':a command removed' "$(printf 'unknown command\na\nb\nc')" "$out"

# :c needs a real line to change -> line-0 rejected
printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':0c X:%p:q!')
check ':0c errors (change needs a real line)' "$(printf '%s\na\nb\nc' "$INVRANGE")" "$out"

printf 'x\ny\nz\n' > "$TMPFILE"
out=$(run_ex ':2ya 97:0pu 97:%p:q!')
check ':0pu pastes above first line' "$(printf 'y\nx\ny\nz')" "$out"

# empty buffer: only the absolute 0 resolves above the (nonexistent) first line
printf '' > "$TMPFILE"
out=$(run_ex ':0i TOP:%p:q!')
check ':0i bootstraps empty buffer' 'TOP' "$out"

printf '' > "$TMPFILE"
out=$(run_ex ':-1i TOP:%p:q!')
check ':-1i errors on empty buffer (no referent)' \
	"$(printf '%s\n%s' "$INVRANGE" "$INVRANGE")" "$out"

# :! filters existing lines; only the absolute 0 bootstraps an empty buffer
printf 'a\nb\n' > "$TMPFILE"
out=$(run_ex ':0!echo HI:%p:q!')
check ':0! errors on non-empty buffer (no line 0)' \
	"$(printf '%s\na\nb' "$INVRANGE")" "$out"

printf '' > "$TMPFILE"
out=$(run_ex ':0!echo HI:%p:q!')
check ':0! fills empty buffer' 'HI' "$out"

printf '' > "$TMPFILE"
out=$(run_ex ':-1!echo HI:%p:q!')
check ':-1! errors on empty buffer (no referent)' \
	"$(printf '%s\n%s' "$INVRANGE" "$INVRANGE")" "$out"

# commands that need a real line (r, w, f>) reject line-0 and out-of-range
# absolute addresses; a rejected :r must leave the current buffer intact
printf 'a\nb\n' > "$TMPFILE"
out=$(run_ex ':0r \!printf X:%p:q!')
check ':0r errors and keeps buffer' "$(printf '%s\na\nb' "$INVRANGE")" "$out"

printf 'a\nb\n' > "$TMPFILE"
out=$(run_ex ':99r \!printf X:%p:q!')
check ':99r out-of-range errors and keeps buffer' \
	"$(printf '%s\na\nb' "$INVRANGE")" "$out"

printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':99w /dev/null:%p:q!')
check ':99w out-of-range errors' "$(printf '%s\na\nb\nc' "$INVRANGE")" "$out"

printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':0f>b:%p:q!')
check ':0f> ranged search errors' "$(printf '%s\na\nb\nc' "$INVRANGE")" "$out"

# a line-0 address is whole-line only: pairing it with a ; column range is
# rejected (the line above the first line has no columns to operate on)
printf 'x\ny\nz\n' > "$TMPFILE"
out=$(run_ex ':2ya 97:0;pu 97:%p:q!')
check ':0;pu rejects column range on line 0' "$(printf '%s\nx\ny\nz' "$INVRANGE")" "$out"

printf 'a\nb\n' > "$TMPFILE"
out=$(run_ex ':0;!echo HI:%p:q!')
check ':0;! rejects column range on non-empty buffer' \
	"$(printf '%s\na\nb' "$INVRANGE")" "$out"

# even on an empty buffer where absolute :0! bootstraps, a ; column range fails
printf '' > "$TMPFILE"
out=$(run_ex ':0;!echo HI:%p:q!')
check ':0;! rejects column range bootstrapping empty buffer' \
	"$(printf '%s\n%s' "$INVRANGE" "$INVRANGE")" "$out"

# :c needs a real line, so line-0 (with or without a column range) is rejected
printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':0;c X:%p:q!')
check ':0;c rejects column range on line 0' "$(printf '%s\na\nb\nc' "$INVRANGE")" "$out"

# :i always ignores the column range, so :0;i stays self-consistent with :0i
printf 'a\nb\nc\n' > "$TMPFILE"
out=$(run_ex ':0;i TOP:%p:q!')
check ':0;i ignores column range, inserts above first line' \
	"$(printf 'TOP\na\nb\nc')" "$out"

# :s full [range] with :p-style o1/o2 column offsets, and the m (region) flag.
# Without m the offsets bound the first/last line and the rest of each line is
# preserved verbatim; with m the whole region is one string, so a pattern may
# span newlines and the region is rewritten as a unit.
S3='aaa bbb aaa\nccc bbb ccc\nddd bbb ddd\n'

printf "$S3" > "$TMPFILE"
out=$(run_ex ':1,3s/bbb/Q/:%p:q!')
check ':s line range substitutes each line' \
	"$(printf 'aaa Q aaa\nccc Q ccc\nddd Q ddd')" "$out"

# o1 alone: search starts at column 4, so the first "aaa" is out of reach
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;4s/aaa/Q/:%p:q!')
check ':s ;o1 starts the search at the column offset' \
	"$(printf 'aaa bbb Q\nccc bbb ccc\nddd bbb ddd')" "$out"

# o1 of 0 is a real offset, not "absent"
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;0s/aaa/Q/:%p:q!')
check ':s ;0 is column 0, not an absent offset' \
	"$(printf 'Q bbb aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

# o1;o2 bound one line; prefix and suffix outside the window are untouched
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;4;8s/bbb/Q/:%p:q!')
check ':s ;o1;o2 bounds the line, keeps prefix and suffix' \
	"$(printf 'aaa Q aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

# a pattern confined to the window cannot reach text outside it
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;4;7s/aaa/Q/:??!p out of window:q!')
check ':s ;o1;o2 window hides text outside it' 'out of window' "$out"

# o1 applies to the first line, o2 to the last; middle lines are unbounded
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;4,3;7s/b/Q/g:%p:q!')
check ':s o1 bounds first line, o2 bounds last line' \
	"$(printf 'aaa QQQ aaa\nccc QQQ ccc\nddd QQQ ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;8,2s/a/Q/g:%p:q!')
check ':s ;o1 on first line only, later lines unbounded' \
	"$(printf 'aaa bbb QQQ\nccc bbb ccc\nddd bbb ddd')" "$out"

# m: the region is one string, so . spans the newline and joins the two lines
printf "$S3" > "$TMPFILE"
out=$(run_ex ':%s/aaa.ccc/X/m:%p:q!')
check ':s m flag matches across a newline' \
	"$(printf 'aaa bbb X bbb ccc\nddd bbb ddd')" "$out"

# m without g is one substitution for the whole region, not one per line
printf "$S3" > "$TMPFILE"
out=$(run_ex ':%s/bbb/Q/m:%p:q!')
check ':s m without g substitutes once per region' \
	"$(printf 'aaa Q aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_ex ':%s/bbb/Q/gm:%p:q!')
check ':s gm substitutes every match in the region' \
	"$(printf 'aaa Q aaa\nccc Q ccc\nddd Q ddd')" "$out"

# m honours the column offsets too; o2 defaults to end of the last line
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;4;8s/bbb/Q/m:%p:q!')
check ':s m with ;o1;o2 rewrites only the bounded window' \
	"$(printf 'aaa Q aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;4,3s/bbb/Q/m:%p:q!')
check ':s m with ;o1 spans to end of the last line' \
	"$(printf 'aaa Q aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;4,3;7s/b/Q/gm:%p:q!')
check ':s gm with ;o1 and ;o2 bounds both ends of the region' \
	"$(printf 'aaa QQQ aaa\nccc QQQ ccc\nddd QQQ ddd')" "$out"

# an offset past end of line leaves nothing to match; the buffer is untouched
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1;99s/aaa/Q/m:??!p nomatch:q!')
check ':s ;o1 past end of line matches nothing' 'nomatch' "$out"

# a region substitution is a single undo unit even though it rewrote 2 lines
printf "$S3" > "$TMPFILE"
out=$(run_ex ':%s/aaa.ccc/X/m:ud:%p:q!')
check ':s m substitution undone in one step' \
	"$(printf 'aaa bbb aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_ex ':%s/aaa.ccc/X/m:ud:rd:%p:q!')
check ':s m substitution redone in one step' \
	"$(printf 'aaa bbb X bbb ccc\nddd bbb ddd')" "$out"

# :s records the cursor position it was issued from, so vi-mode u returns there
# instead of jumping to the first edited line. :ud discards row/off, so this is
# only visible through vi mode: X marks where the cursor ended up after undo.
printf "$S3" > "$TMPFILE"
out=$(run_vi "$(printf '3G\\:1,2s/bbb/Q/\nuIX\033')")
check ':s undo returns the cursor to where the command was issued' \
	"$(printf 'aaa bbb aaa\nccc bbb ccc\nXddd bbb ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_vi "$(printf '3G\\:1,2s/bbb/Q/m\nuIX\033')")
check ':s m undo returns the cursor to where the command was issued' \
	"$(printf 'aaa bbb aaa\nccc bbb ccc\nXddd bbb ddd')" "$out"

# redo reads its position from the last history entry of the sequence, so the
# closing marker has to be written for a region substitution too
printf "$S3" > "$TMPFILE"
out=$(run_vi "$(printf '3G\\:1,2s/bbb/Q/\nu\022IX\033')")
check ':s redo returns the cursor to where the command was issued' \
	"$(printf 'aaa Q aaa\nccc Q ccc\nXddd bbb ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_vi "$(printf '3G\\:1,2s/bbb/Q/m\nu\022IX\033')")
check ':s m redo returns the cursor to where the command was issued' \
	"$(printf 'aaa Q aaa\nccc bbb ccc\nXddd bbb ddd')" "$out"

# :s on a register — a trailing digit run, and only flags before it, redirects
# the substitution to that register instead of the buffer. The register is one
# flat string, so m is implied and the range is deferred (and then rejected).
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1,3ya 97:s/bbb/QQ/ 97:%d:0pu 97:%p:q!')
check ':s on a register substitutes once for the whole register' \
	"$(printf 'aaa QQ aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_ex ':1,3ya 97:s/bbb/QQ/g 97:%d:0pu 97:%p:q!')
check ':s g on a register substitutes every match' \
	"$(printf 'aaa QQ aaa\nccc QQ ccc\nddd QQ ddd')" "$out"

printf "$S3" > "$TMPFILE"
out=$(run_ex ':1,3ya 97:s/aaa.ccc/X/ 97:%d:0pu 97:%p:q!')
check ':s on a register matches across a newline' \
	"$(printf 'aaa bbb X bbb ccc\nddd bbb ddd')" "$out"

# the buffer is left alone; only the register changed
printf "$S3" > "$TMPFILE"
out=$(run_ex ':1,3ya 97:s/bbb/QQ/g 97:%p:q!')
check ':s on a register leaves the buffer untouched' \
	"$(printf 'aaa bbb aaa\nccc bbb ccc\nddd bbb ddd')" "$out"

# a register substitution is not an undo unit, so u reaches the buffer edit
printf 'a b\nc d\n' > "$TMPFILE"
out=$(run_ex ':%ya 97:%s/a/Z/:s/b/Y/ 97:ud:%p:0pu 97:%p:q!')
check ':s on a register records no undo entry' \
	"$(printf 'a b\nc d\na Y\nc d\na b\nc d')" "$out"

# deferring the range is what lets a register be edited with no lines at all
printf 'p q\n' > "$TMPFILE"
out=$(run_ex ':%ya 97:%d:s/q/Z/ 97:0pu 97:%p:q!')
check ':s on a register works on an empty buffer' 'p Z' "$out"

# a range cannot apply to a register, so it is rejected rather than ignored
printf 'a b\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:1s/a/X/ 97:q!')
check ':s rejects a range with a register' 'register takes no range' "$out"

printf 'a b\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:s/a/X/ 98:q!')
check ':s reports an unset register' 'uninitialized register' "$out"

# no match in the register is an error, so ?? gates on it
printf 'a b\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:s/zzz/X/ 97:??!p nomatch:q!')
check ':s on a register errors when nothing matches' 'nomatch' "$out"

# and a register substitution that did match must not report one, or ?? cannot
# tell the two apart
printf 'a b\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:s/a/X/ 97:??!p nomatch:p done:q!')
check ':s on a register reports no error when it matched' 'done' "$out"

# only flags may precede the digits; anything else means there is no register
# spec and the tail is the junk it has always been, so the buffer is edited
printf 'a b\n' > "$TMPFILE"
out=$(run_ex ':1ya 97:s/a/X/ 97 junk:%p:0pu 97:%p:q!')
check ':s digit run with trailing junk is not a register spec' \
	"$(printf 'X b\na b\nX b')" "$out"

# '[ and '] span the whole region, not just the last line lbuf_edit touched
printf "$S3" > "$TMPFILE"
out=$(run_ex ":%s/aaa.ccc/X/m:'91p:'93p:q!")
check ":s m sets '[ and '] to the region bounds" \
	"$(printf 'aaa bbb X bbb ccc\nddd bbb ddd')" "$out"

# ──────────────────────────────────────────────────────────────────────────────
# :s with a target group (:grp N)
# A match whose target group did not participate is not substituted, but the
# text it covers is ordinary line content and must survive the rewrite — before
# the first substitution, between two of them, and after the last one.
# ──────────────────────────────────────────────────────────────────────────────

printf 'aXbYa\n' > "$TMPFILE"
out=$(run_ex ':grp 1:%s/(a)|b/Z/g:grp:%p:q!')
check ':s grp keeps a skipped match between two substitutions' 'ZXbYZ' "$out"

printf 'bXa\n' > "$TMPFILE"
out=$(run_ex ':grp 1:%s/(a)|b/Z/g:grp:%p:q!')
check ':s grp keeps a skipped match before the first substitution' 'bXZ' "$out"

printf 'aXb\n' > "$TMPFILE"
out=$(run_ex ':grp 1:%s/(a)|b/Z/g:grp:%p:q!')
check ':s grp keeps a skipped match after the last substitution' 'ZXb' "$out"

printf 'bXaXb\n' > "$TMPFILE"
out=$(run_ex ':grp 1:%s/(a)|b/Z/g:grp:%p:q!')
check ':s grp keeps skipped matches on both sides' 'bXZXb' "$out"

# every match skipped is no substitution at all, so the line is untouched
printf 'bXb\n' > "$TMPFILE"
out=$(run_ex ':grp 1:%s/(a)|b/Z/g:??!p nomatch:q!')
check ':s grp errors when the group never participates' 'nomatch' "$out"

# same rule across a region: skipped matches survive on their own lines
printf 'a\nb\na\n' > "$TMPFILE"
out=$(run_ex ':grp 1:%s/(a)|b/Z/gm:grp:%p:q!')
check ':s gm grp keeps a skipped match inside the region' \
	"$(printf 'Z\nb\nZ')" "$out"

# ──────────────────────────────────────────────────────────────────────────────
# :s g and the ^ anchor
# Every search after the first resumes mid-string, so ^ must not match there.
# ──────────────────────────────────────────────────────────────────────────────

printf 'aaa\n' > "$TMPFILE"
out=$(run_ex ':%s/^a/X/g:%p:q!')
check ':s g does not re-anchor ^ after a match' 'Xaa' "$out"

printf 'ab\n' > "$TMPFILE"
out=$(run_ex ':%s/^/S/g:%p:q!')
check ':s g matches a bare ^ once per line' 'Sab' "$out"

# only one branch of the alternation is anchored; b stays matchable anywhere
printf 'aba\n' > "$TMPFILE"
out=$(run_ex ':%s/^a|b/X/g:%p:q!')
check ':s g keeps an unanchored branch after ^ stops matching' 'XXa' "$out"

# a plain g region re-anchors on every line, since each line is its own string
printf 'aaa\naaa\n' > "$TMPFILE"
out=$(run_ex ':%s/^a/X/g:%p:q!')
check ':s g anchors ^ at the start of each line' \
	"$(printf 'Xaa\nXaa')" "$out"

# under m the region is one string, so ^ only matches at the region start
printf 'aaa\naaa\n' > "$TMPFILE"
out=$(run_ex ':%s/^a/X/gm:%p:q!')
check ':s gm anchors ^ at the region start only' \
	"$(printf 'Xaa\naaa')" "$out"

# REG_NEWLINE must survive the switch to REG_NOTBOL, or $ stops matching
printf 'aaa\n' > "$TMPFILE"
out=$(run_ex ':%s/a$/X/g:%p:q!')
check ':s g still matches $ after the first search' 'aaX' "$out"

# ──────────────────────────────────────────────────────────────────────────────
# :s g and zero-length matches at the line end
# The trailing newline is a match position like any other: REG_NEWLINE makes it
# a terminator, so only a zero-width match can land there and nothing can run
# past it. g and gm must agree about it.
# ──────────────────────────────────────────────────────────────────────────────

printf 'ab\n' > "$TMPFILE"
out=$(run_ex ':%s/x*/-/g:%p:q!')
check ':s g substitutes a zero-length match at the line end' '-a-b-' "$out"

printf 'ab\ncd\n' > "$TMPFILE"
out=$(run_ex ':%s/x*/-/g:%p:q!')
check ':s g reaches the line end on every line' \
	"$(printf -- '-a-b-\n-c-d-')" "$out"

printf 'ab\ncd\n' > "$TMPFILE"
out=$(run_ex ':%s/x*/-/gm:%p:q!')
check ':s gm agrees with g on the line end' \
	"$(printf -- '-a-b-\n-c-d-')" "$out"

# a zero-length match after a non-empty one is still a match of its own
printf 'axb\n' > "$TMPFILE"
out=$(run_ex ':%s/x*/-/g:%p:q!')
check ':s g takes a zero-length match adjoining a non-empty one' '-a--b-' "$out"

printf '\n' > "$TMPFILE"
out=$(run_ex ':%s/x*/-/g:%p:q!')
check ':s g substitutes a zero-length match on an empty line' '-' "$out"

# the window ends at a NUL, and the engine reports no match on an empty string,
# so a bounded region has no match position at its end
printf 'axbxc\n' > "$TMPFILE"
out=$(run_ex ':1;1;4s/x*/-/g:%p:q!')
check ':s g bounded by o2 has no match at the window end' 'a--b-c' "$out"

# dropping the newline test from the loop must not let m repeat
printf 'xx\nxx\n' > "$TMPFILE"
out=$(run_ex ':%s/x/Y/m:%p:q!')
check ':s m still substitutes once across the region' \
	"$(printf 'Yx\nxx')" "$out"

# ──────────────────────────────────────────────────────────────────────────────
# :fr register search and the range cursor
# The range maps a register offset back onto the buffer, so the search has to
# start from a position that is inside the range. A cursor outside of it is
# pinned to the range start; only an offset that walks past the range end is
# a failure.
# ──────────────────────────────────────────────────────────────────────────────

printf 'aaa\nbbb\nccc\nddd\neee\n' > "$TMPFILE"
out=$(run_ex ':3,5ya97:fr 97:4:3,5f>eee:.p:q')
check ':fr cursor inside the range searches from the cursor' 'eee' "$out"

printf 'aaa\nbbb\nccc\nddd\neee\n' > "$TMPFILE"
out=$(run_ex ':3,5ya97:fr 97:1:3,5f>ddd:.p:q')
check ':fr cursor before the range starts at the range start' 'ddd' "$out"

printf 'aaa\nbbb\nccc\nddd\neee\n' > "$TMPFILE"
out=$(run_ex ':1,3ya97:fr 97:5:1,3f>bbb:.p:q')
check ':fr cursor after the range starts at the range start' 'bbb' "$out"

# o1 bounds the first row, so a cursor left of it is outside the range as well
printf 'int a\nint b\nint c\n' > "$TMPFILE"
out=$(run_ex ':1;2,3;4ya97:fr 97:1:1;2,3;4f>int b:.p:q')
check ':fr cursor left of o1 starts at the range start' 'int b' "$out"

# o2 is the last position in the range, so the :f+ increment past it exhausts
# the search; it does not wrap back to the range start
printf 'int a\nint b\nint c\n' > "$TMPFILE"
out=$(run_ex ':%f>t c:1;0,3;2ya97:fr 97:1;0,3;2f+int:??!p exhausted:.p:q')
check ':fr f+ past the range end fails instead of wrapping' \
	"$(printf 'exhausted\nint c')" "$out"

printf '\n%s\n' '─── Summary ──────────────────────────────────────────────────────────────────'

printf '\nResults: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
