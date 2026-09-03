#include "kmap.h"

/* access mode of new files */
const int conf_mode = 0600;
#define FTGEN(ft) static char ft##_ft[] = #ft;
#define FT(ft) ft##_ft
FTGEN(c) FTGEN(roff) FTGEN(tex) FTGEN(mbox)
FTGEN(mk) FTGEN(sh) FTGEN(py) FTGEN(js)
FTGEN(html) FTGEN(diff) FTGEN(go) FTGEN(md)

char _ft[] = "/";	/* default hl */
char fm_ft[] = "/fm";	/* file manager */
char n_ft[] = "/#";	/* numbers highlight for ^v */
char nn_ft[] = "/##";	/* numbers highlight for # */
char ac_ft[] = "/ac";	/* autocomplete dropdown */
char ex_ft[] = "/ex";	/* ex mode (is never '\n' terminated) */
char vs_ft[] = "/vs";	/* vi search prompt (is never '\n' terminated) */
char bar_ft[] = "/-";	/* status bar (is never '\n' terminated) */
char fuzz_ft[] = "/f";	/* fuzzy search prompt (is never '\n' terminated) */
char msg_ft[] = "/>";	/* ex message (is never '\n' terminated) */

struct filetype fts[] = {
	{FT(c), "\\.(c|h|cpp|hpp|cc|cs)$"},			/* C */
	{FT(roff), "\\.(ms|tr|roff|tmac|txt|[1-9])$"},		/* troff */
	{FT(tex), "\\.tex$"},					/* tex */
	{FT(mbox), "letter$|mbox$|mail$"},			/* email */
	{FT(mk), "[Mm]akefile$|\\.mk$"},			/* makefile */
	{FT(sh), "\\.(ba|z)?sh$|(ba|z|k)shrc$|profile$"},	/* shell script */
	{FT(py), "\\.py$"},					/* python */
	{FT(js), "\\.js$"},					/* javascript */
	{FT(html), "\\.(html?|css)$"},				/* html,css */
	{FT(diff), "\\.(patch|diff|rej)$"},			/* diff */
	{FT(go), "\\.go$"},					/* go */
	{FT(md), "\\.md$"},					/* markdown */
	{_ft, NULL},
	{fm_ft, NULL},
	{n_ft, NULL},
	{nn_ft, NULL},
	{ac_ft, NULL},
	{ex_ft, NULL},
	{vs_ft, NULL},
	{bar_ft, NULL},
	{fuzz_ft, NULL},
	{msg_ft, NULL}
};
const int ftslen = LEN(fts);

#define NA	0	/* no attribute */
#define RE	1	/* red */
#define GR	2	/* green */
#define YE	3	/* yellow */
#define BL	4	/* blue */
#define MA	5	/* magenta */
#define CY	6	/* cyan */
#define AY	7	/* gray */
#define AY1	8	/* bright gray */
#define RE1	9	/* bright red */
#define GR1	10	/* bright green */
#define YE1	11	/* bright yellow */
#define BL1	12	/* bright blue */
#define MA1	13	/* bright magenta */
#define CY1	14	/* bright cyan */
#define WH1	15	/* bright white */

#define A(...) (int[]){__VA_ARGS__}

/* At least 1 entry is required in this struct for fallback */
/* lbuf lines are *always "\n\0" terminated, for $ to work one needs to account for '\n' too */
struct highlight hls[] = {
	{_ft, NULL, A(CY1 | SYN_BD), 1, 2},  /* <-- optional, used by hll if set */
	{_ft, NULL, A(RE1 | SYN_BGMK(GR1)), 0, 3}, /* <-- optional, used by hlp if set */
	{_ft, NULL, A(RE1), 0, 1}, /* <-- optional, used by hlw if set */

	{FT(c), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(c), "(/\\*(?:(?!^\\*/).)*)|((?:(?!^/\\*)(?!^//).)*\\*/\
(?#-1)(?<\".*\\*/.*(?:\"|\\\\\n$)))|(//.*\\*/)",
		A(BL | SYN_IT | SYN_SATT, 2, NA | SYN_BATT, BL | SYN_BATT,
		BL | SYN_BLK, SYN_BSE | SYN_BEDP, BL | SYN_BLK, SYN_BSE | SYN_BSD,
		BL | SYN_BLK, SYN_BE | SYN_BEDP)},
	{FT(c), "\\<(?:signed|unsigned|char|short|int|[a-z0-9_]+_t|FILE|DIR|long|f(?:loat|64|32)|\
double|void|enum|union|typedef|static|extern|register|struct|s(?:64|32|16|8)|u(?:64|32|16|8)|b32|\
bool|const|inline|restrict|auto|(true|false|_?_?asm_?_?|mem(?:set|cpy|cmp)|free|(?:posix_)?memalign|\
(?:m|c|v|aligned_)alloc|realloc(?:f|array)?|NULL|std(?:in|out|err)|errno)|\
(return|for|while|if|else|do|sizeof|goto|switch|case|default|break|continue))\\>",
		A(GR1, BL1 | SYN_BD, YE1)},
	{FT(c), "//.*", A(BL | SYN_IT | SYN_SATT, 2, NA | SYN_BATT, BL | SYN_BATT)},
	{FT(c), "\"(?#2)(?<^\\\\)(?:[^\"\\\\]|\\\\.)*\"", A(MA)},
	{FT(c), "#[ \t]*(?:[a-zA-Z0-9_]+([ \t]*<.*>)?)", A(CY, MA)},
	{FT(c), "[a-zA-Z0-9_]+(?=^\\()", A(SYN_BD)},
	{FT(c), "'(?:[^\\\\]|\\\\.|\\\\x[0-9a-fA-F]{1,2}|\\\\[0-9]+?)'", A(MA)},
	{FT(c), "[-+.]?\\<(?:0[xX][0-9a-fA-F]+|[0-9]+\\.?[0-9]*(?:[eE][-+]?[0-9]+)?)([fFlLuU]{0,3})\\>",
		A(RE1, RE)},
	{FT(c), "(\"[^\"]*\\\\\n$)|^(.*\"(?!\\\\\n$))(?:.*?(/[/*].*))?",
		A(MA | SYN_IGN, MA | SYN_SATT | SYN_OWR | SYN_BLK, 1, NA, SYN_BSE | SYN_BED,
		MA | SYN_OWR | SYN_EATT | SYN_BLK, 1, NA, SYN_BSE | SYN_BSD, BL | SYN_IT), 1},
	{FT(c), "^.+\\\\\n$", A(CY1 | SYN_EATT | SYN_OATT, 2, NA, BL, 1, NA), 2},
	{FT(c), NULL, A(RE1), 0, 1},
	{FT(c), NULL, A(RE1 | SYN_BGMK(BL1)), 0, 3},
	{FT(c), "(\\?).+?(:)", A(SYN_IGN, YE | SYN_SATT, 2, NA, CY1,
				YE | SYN_SATT, 2, NA, CY1), 5},

	{FT(roff), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(roff), "^[.'][ \t]*(([sS][hH].*)|(de) (.*)|([^ \t\\\\]{2,}))?.*",
		A(BL, NA, MA | SYN_BD, BL | SYN_BD, MA | SYN_BD, BL | SYN_BD), 1},
	{FT(roff), NULL, A(RE1), 0, 1},
	{FT(roff), "\\\\\".*", A(GR | SYN_IT)},
	{FT(roff), "\\\\{1,2}[*$fgkmns](?:[^[\\(]|\\(..|\\[[^\\]]*\\])", A(YE)},
	{FT(roff), "\\\\(?:[^[\\(*$fgkmns]|\\(..|\\[[^\\]]*\\])", A(YE)},
	{FT(roff), "\\$[^$]+\\$", A(YE)},

	{FT(tex), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(tex), NULL, A(RE1), 0, 1},
	{FT(tex), "\\\\[^[{ \t]+(\\[([^\\]]+)\\])?(\\{([^}]*)})?",
		A(BL | SYN_BD, NA, YE, NA, MA)},
	{FT(tex), "\\$[^$]+\\$", A(YE)},
	{FT(tex), "%.*", A(GR | SYN_IT)},

	{FT(mbox), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(mbox), NULL, A(RE1), 0, 1},
	{FT(mbox), "^From .*20..\n$", A(CY | SYN_BD)},
	{FT(mbox), "^Subject: (.*)", A(CY | SYN_BD, BL | SYN_BD)},
	{FT(mbox), "^From: (.*)", A(CY | SYN_BD, GR | SYN_BD)},
	{FT(mbox), "^To: (.*)", A(CY | SYN_BD, MA | SYN_BD)},
	{FT(mbox), "^Cc: (.*)", A(CY | SYN_BD, MA | SYN_BD)},
	{FT(mbox), "^[-A-Za-z]+: .+", A(CY | SYN_BD)},
	{FT(mbox), "^> .*", A(GR | SYN_IT)},

	{FT(mk), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(mk), NULL, A(RE1), 0, 1},
	{FT(mk), "([A-Za-z0-9_]*)[ \t]*:?=", A(NA, YE)},
	{FT(mk), "\\$[\\({][a-zA-Z0-9_]+[\\)}]|\\$\\$", A(YE)},
	{FT(mk), "#.*", A(GR | SYN_IT)},
	{FT(mk), "([A-Za-z_%.\\-]+):", A(NA, SYN_BD)},

	{FT(sh), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(sh), NULL, A(RE1), 0, 1},
	{FT(sh), "\\<(?:break|case|continue|do|done|elif|else|esac|fi|for|if|in|then|until|while)\\>",
		A(MA | SYN_BD)},
	{FT(sh), "[ \t](#.*)|^(#.*)", A(NA, GR | SYN_IT, GR | SYN_IT)},
	{FT(sh), "\"(?:[^\"\\\\]|\\\\.)*\"", A(BL)},
	{FT(sh), "`(?:[^`\\\\]|\\\\.)*`", A(BL)},
	{FT(sh), "'[^']*'", A(BL)},
	{FT(sh), "\\$(?:\\{[^}]+}|[a-zA-Z_0-9]+|[!#$?*@-])", A(RE)},
	{FT(sh), "^([a-zA-Z_0-9]* *\\(\\)) *\\{", A(NA, SYN_BD)},
	{FT(sh), "^\\. .*", A(SYN_BD)},

	{FT(py), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(py), NULL, A(RE1), 0, 1},
	{FT(py), "#.*", A(GR)},
	{FT(py), "\\<(?:and|break|class|continue|def|del|elif|else|except|finally|\
for|from|global|if|import|in|is|lambda|not|or|pass|print|raise|return|try|while)\\>", A(MA)},
	{FT(py), "[a-zA-Z0-9_]+(?=^\\()", A(SYN_BD)},
	{FT(py), "\"{3}.*?\"{3}", A(CY)},
	{FT(py), "(?:(?:(?!^\"\"\").)*\"{3}\n$)|(?:\"{3}(?:(?!^\"\"\").)*)|\"{3}",
		A(CY | SYN_BLK, SYN_BSE | SYN_BSDP | SYN_BEDP)},
	{FT(py), "[\"](?:\\\\\"|[^\"])*?[\"]", A(BL)},
	{FT(py), "['](?:\\\\'|[^'])*?[']", A(BL)},

	{FT(js), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(js), "(/\\*(?:(?!^\\*/).)*)|((?:(?!^/\\*).)*\\*/(?![\"'`]))",
		A(GR1 | SYN_IT, GR1 | SYN_BLK, SYN_BSE | SYN_BEDP, GR1 | SYN_BLK, SYN_BSE | SYN_BSD)},
	{FT(js), NULL, A(RE1), 0, 1},
	{FT(js), "\\<(?:abstract|arguments|await|boolean|break|byte|case|catch|char|class|\
const|continue|debugger|default|delete|do|double|else|enum|eval|export|extends|false|\
final|finally|float|for|function|goto|if|implements|import|in|instanceof|int|interface|\
let|long|native|new|null|package|private|protected|public|return|short|static|super|\
switch|synchronized|this|throw|throws|transient|true|try|typeof|var|void|volatile|\
while|with|yield|(Array|Date|hasOwnProperty|Infinity|isFinite|isNaN|isPrototypeOf|\
length|Math|NaN|name|Number|Object|prototype|String|toString|undefined|valueOf))\\>",
		A(BL1, CY | SYN_BD)},
	{FT(js), "[-+]?\\<(?:0[xX][0-9a-fA-F]+|[0-9]+)\\>", A(RE1)},
	{FT(js), "//.*", A(GR1 | SYN_IT)},
	{FT(js), "'(?:[^'\\\\]|\\\\.)*'", A(MA)},
	{FT(js), "\"(?:[^\"\\\\]|\\\\.)*\"", A(MA)},
	{FT(js), "`(?:[^`\\\\]|\\\\.)*`", A(MA)},

	{FT(html), "<(/)?(?:[^>](?:\".*?\")*(?:'.*?')*(?:<.*?>)*)+>", A(YE, MA1), 1},
	{FT(html), "^(?:[ \t.,#*:a-zA-Z0-9_-]+(?:\\(.*\\))*(?:\\[.*\\])*[ \t+~>]?)*(?=^\\{)", A(WH1), 2},
	{FT(html), ">([^<]+)<", A(SYN_SKIP, 69 | SYN_MK), 2},
	{FT(html), "\\<(?#1)(?<^[-.])(?:accept|accept|action|align|alt|async|autocomplete|\
autofocus|autoplay|bgcolor|border|charset|checked|cite|color|cols|colspan|content|controls|\
coords|data|datetime|default|defer|dirname|disabled|download|enctype|for|form|formaction|\
headers|height|high|href|hreflang|http|ismap|kind|label|list|loop|low|max|maxlength|media|\
method|min|multiple|muted|name|novalidate|onabort|onafterprint|onbeforeprint|onbeforeunload|\
onblur|oncanplay|oncanplaythrough|onchange|onclick|oncontextmenu|oncopy|oncuechange|oncut|\
ondblclick|ondrag|ondragend|ondragenter|ondragleave|ondragover|ondragstart|ondrop|\
ondurationchange|onemptied|onended|onerror|onfocus|onhashchange|oninput|oninvalid|\
onkeydown|onkeypress|onkeyup|onload|onloadeddata|onloadedmetadata|onloadstart|onmousedown|\
onmousemove|onmouseout|onmouseover|onmouseup|onmousewheel|onoffline|ononline|onpagehide|\
onpageshow|onpaste|onpause|onplay|onplaying|onpopstate|onprogress|onratechange|onreset|\
onresize|onscroll|onsearch|onseeked|onseeking|onselect|onstalled|onstorage|onsubmit|\
onsuspend|ontimeupdate|ontoggle|onunload|onvolumechange|onwaiting|onwheel|open|optimum|\
pattern|placeholder|popovertarget|popovertargetaction|poster|preload|readonly|rel|\
required|reversed|rows|rowspan|sandbox|scope|selected|shape|size|sizes|span|src|accesskey|\
class|contenteditable|data-[a-z]*|dir|draggable|enterkeyhint|hidden|id|inert|inputmode|\
lang|popover|spellcheck|style(?=^=)|tabindex|title(?=^=)|translate|srcdoc|srclang|srcset|\
start|step|target|type|usemap|value|width|wrap|([dD](?:octype|OCTYPE)|a|abbr|address|area|\
article|aside|audio|b|base|bdi|bdo|blockquote|body|br|button|canvas|caption|cite|code|col|\
colgroup|data|datalist|dd|del|details|dfn|dialog|div|dl|dt|em|embed|fieldset|figcaption|\
figure|footer|form|h[1-6]|head|header|hgroup|hr|html|i|iframe|img|input|ins|kbd|label|\
legend|li|link|main|map|mark|menu|meta|meter|nav|noscript|object|ol|optgroup|option|output|\
p|param|picture|pre|progress|q|rp|rt|ruby|s|samp|script|search|section|select|small|source|\
span|strong|style|sub|summary|sup|svg|table|tbody|td|template|textarea|tfoot|th|thead|time|\
title|tr|track|u|ul|var|video|wbr))\\>(?!^-)",
		A(GR | SYN_OATT | SYN_OWR, 1, YE, CY | SYN_OATT | SYN_OWR, 3, YE, GR, WH1)},
	{FT(html), "^[ \t]*@\\<[a-z-]+\\>", A(BL1 | SYN_BD)},
	{FT(html), "::?\\<[a-z-]+(?=^[, \t{\\(\\[])", A(MA1 | SYN_OATT | SYN_OWR, 1, WH1)},
	{FT(html), "\"(?:[^\"\\\\]|\\\\.)*\"", A(BL1)},
	{FT(html), "'(?:[^'\\\\]|\\\\.)*'", A(MA)},
	{FT(html), "&[a-zA-Z0-9_#]+;", A(MA)},
	{FT(html), "(<!--(?:(?!^-->).)*)|((?:(?!^<!--).)*-->)",
		A(MA | SYN_IGN, MA | SYN_BLK | SYN_SATT, 3, NA, 69, YE, SYN_BSE | SYN_BEDP,
		MA | SYN_EATT | SYN_BLK, 3, NA, 69, YE, SYN_BSE | SYN_BSD), 1},
	{FT(html), "(/\\*(?:(?!^\\*/).)*)|((?:(?!^/\\*).)*\\*/)",
		A(MA | SYN_IT | SYN_IGN, MA | SYN_IT | SYN_SATT | SYN_BLK, 2, NA, AY1,
		SYN_BSE | SYN_BEDP, MA | SYN_IT | SYN_EATT | SYN_BLK, 2, NA, AY1, SYN_BSE | SYN_BSD), 2},
	{FT(html), "((\\{)[^}]*)|((?:^[^{]*)?(}))",
		A(AY1 | SYN_IGN, AY1 | SYN_SATT, 1, NA, GR1 | SYN_BLK, SYN_BN | SYN_BP | SYN_BSE | SYN_BEDP,
		AY1 | SYN_EATT, 1, NA, GR1 | SYN_BLK, SYN_BN | SYN_BP | SYN_BSE | SYN_BSD), 2},
	{FT(html), "(?:#\\<[A-Fa-f0-9]+\\>)|[-+]?\\<(?:0[xX][0-9a-fA-F]+|[0-9]+\
(?:\\.[0-9]+)?(?:em|rem|px|pt|pc|cm|mm|in|ch|ex|vw|vh|vmin|vmax|dvh|dvw|svh|svw|lvh|lvw|\
fr|deg|rad|turn|grad|ms|s|hz|khz|dpi|dpcm|dppx|%|))\\>", A(RE1 | SYN_ATT, 4, 69, NA, AY1, YE), 3},
	{FT(html), "^[^{]*(?=^\\{)", A(SYN_SKIP), 4},
	{FT(html), "(?:^|[ \t{;])([^\t -,.-/:-@[-^{-~]+:)\
(?:[ \t]*(fixed|absolute|relative|.+!important)|.+?)(?:(?=^;)|(?=^}\n))",
		A(NA | SYN_IGN, YE | SYN_OWR | SYN_EATT, 2, AY1 | SYN_BATT, GR1 | SYN_BATT, MA1), 4},
	{FT(html), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(html), NULL, A(RE1), 0, 1},
	{FT(html), NULL, A(AY | SYN_BGMK(RE1)), 0, 3},

	{FT(diff), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(diff), "^-.*", A(RE)},
	{FT(diff), "^\\+.*", A(GR)},
	{FT(diff), "^@.*", A(CY)},
	{FT(diff), "^diff .*", A(SYN_BD)},

	{FT(go), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(go), "(/\\*(?:(?!^\\*/).)*)|((?:(?!^/\\*).)*\\*/(?#-1)(?<\".*\\*/.*\"))",
		A(BL | SYN_IT, BL | SYN_BLK, SYN_BSE | SYN_BEDP, BL | SYN_BLK, SYN_BSE | SYN_BSD)},
	{FT(go), NULL, A(RE1 | SYN_BGMK(BL1)), 0, 3},
	{FT(go), NULL, A(RE1), 0, 1},
	{FT(go), "\\<(?:any|bool|byte|comparable|complex64|complex128|error|float32|float64|\
int|int8|int16|int32|int64|rune|string|uint|uint8|uint16|uint32|uint64|uintptr|\
chan|interface|map|struct|(true|false|iota|nil|append|cap|close|complex|copy|delete|imag|\
len|make|new|panic|print|println|real|recover)|(break|case|const|continue|default|defer|\
else|fallthrough|for|func|go|goto|if|import|package|range|\
return|select|switch|type|var))\\>", A(GR1, BL1 | SYN_BD, YE1)},
	{FT(go), "//.*", A(BL | SYN_IT)},
	{FT(go), "\"(?:[^\"\\\\]|\\\\.)*\"", A(MA)},
	{FT(go), "`[^`]*`", A(MA)},
	{FT(go), "[a-zA-Z0-9_]+(?=^\\()", A(SYN_BD)},
	{FT(go), "'(?:[^\\\\]|\\\\.|\\\\x[0-9a-fA-F]{2}|\\\\u[0-9a-fA-F]{4}|\\\\U[0-9a-fA-F]{8}|\\\\[0-7]{3})'", A(MA)},
	{FT(go), "[-+.]?\\<(?:0[xX][0-9a-fA-F]+|0[oO][0-7]+|0[bB][01]+|[0-9]+\\.?[0-9eEi]*|[0-9]+)\\>", A(RE1)},

	{FT(md), NULL, A(CY1 | SYN_BD), 1, 2},
	{FT(md), NULL, A(RE1), 0, 1},
	{FT(md), "^# .*", A(RE | SYN_BD)},
	{FT(md), "^## .*", A(GR | SYN_BD)},
	{FT(md), "^### .*", A(YE | SYN_BD)},
	{FT(md), "^#### .*", A(BL | SYN_BD)},
	{FT(md), "^##### .*", A(MA | SYN_BD)},
	{FT(md), "^###### .*", A(CY | SYN_BD)},
	{FT(md), "[*][*][^*]+[*][*]|__[^_]+__", A(YE | SYN_BD)},
	{FT(md), "[*][^*]+[*]|_[^_]+_", A(YE | SYN_IT)},
	{FT(md), "~~[^~]+~~", A(AY | SYN_IT)},
	{FT(md), "^[ \t]*```([a-z0-9]+)?\n$",
		A(CY | SYN_BLK, SYN_BSE | SYN_BSDP | SYN_BEDP,
		CY | SYN_BD | SYN_BLK, SYN_BE | SYN_BED)},
	{FT(md), "`[^`]*`", A(CY)},
	{FT(md), "^(---+|\\*\\*\\*+|___+)\n$", A(BL)},
	{FT(md), "(  )\n$", A(SYN_IGN, SYN_BGMK(MA))},
	{FT(md), "^>[ \n].*", A(MA)},
	{FT(md), "^[ \t]*[*\\-+] ", A(YE)},
	{FT(md), "^[ \t]*[0-9]+[.] ", A(YE)},
	{FT(md), "[[][^[\\]]+[\\]]\\([^\\(\\)]+\\)", A(CY)},
	{FT(md), "![[][^[\\]]+[\\]]\\([^\\(\\)]+\\)", A(MA)},

	{fm_ft, "^.+\n$", A(AY1), 1},
	{fm_ft, "(^\\.?\\.?)/|(\\.\\.(/))|(?:[^/]+/)+", A(CY, BL, BL, CY), 2},
	{fm_ft, "[^/]*\\.sh\n$", A(GR)},
	{fm_ft, "[^/]*(?:\\.c|\\.h|\\.cpp|\\.cc)\n$", A(MA)},
	{fm_ft, "[^/]*\\.go\n$", A(CY)},

	{n_ft, "[0lewEW]", A(CY1 | SYN_BD)},
	{n_ft, "1([ \t]*[1-9][ \t]*)9", A(RE1, MA1 | SYN_BD)},
	{n_ft, "9[ \t]*([1-9][ \t]*)1", A(RE1, MA1 | SYN_BD)},
	{n_ft, "[1-9]", A(RE1)},

	{nn_ft, "[0-9]+", A(RE1 | SYN_BD)},

	{ac_ft, "[^ \t-/:-@[-^{-~]+(?:(\n$)|\n)|\n|([^\n]+(\n))",
		A(NA, SYN_BGMK(RE1), SYN_BGMK(AY1), SYN_BGMK(AY))},
	{ac_ft, "[^ \t-/:-@[-^{-~]+$|(.+$)", A(NA, SYN_BGMK(AY1))},

	{ex_ft, ".+", A(AY1 | SYN_BD), 1},
	{ex_ft, ":[ \t]*((((?:\\|(?:[^|\\\\]|\\\\.?)*\\|?[ \t]*)*(?:(?:<(?:[^<\\\\]|\\\\.?)*<?|>(?:[^>\\\\]|\\\\.?)*>?)|\
(?:'[0-9]+)|([.%$]|[0-9 \t]*)?))(?:([-*-+/%])[ \t]*[0-9]+[ \t]*)*(?:[ \t]*\\|(?:[^|\\\\]|\\\\.?)*\\|?[ \t]*)*)[ \t]*\
(?:([,;]#?)[ \t]*((?:\\|(?:[^|\\\\]|\\\\.?)*\\|?[ \t]*)*(?:(?:<(?:[^<\\\\]|\\\\.?)*<?|>(?:[^>\\\\]|\\\\.?)*>?)|\
(?:'[0-9]+)|([.$]|[0-9 \t]*)?))(?:([-*-+/%])[ \t]*([0-9]+)[ \t]*)*(?:[ \t]*\\|(?:[^|\\\\]|\\\\.?)*\\|?)*[ \t]*)*)\
((pac|pr|ai|ish|err|fr|ic|grp|mpt|rr|shape|seq|ts|td|order|hl[lwpr]?|left|lim|led|vis)\
|[@&!dmj]|=\\?{0,1}|\\?{1,2}[?!]?|b[psx]?|p[uh]?|ac|e[f!]?!?|f[-+><tdp]?|inc|i|sc!?|\
(?:g!?|s)[ \t]?(.)?|q!?|reg?\\+?|rd?|w(?:q!|[q!])?|u[czbd]|x!?|ya[!+]?|cm!?|cd?)?",
		A(BL1 | SYN_BD, RE, RE, RE, RE, WH1, MA1, RE, RE, WH1, RE, GR1, CY1, MA1)},
	{ex_ft, "\\\\(.)", A(AY1 | SYN_BD, YE)},
	{ex_ft, "!(?:[^!\\\\]|\\\\.?)*!?|%(?:#|[0-9]+|@([0-9]+))?", A(WH1 | SYN_BD, CY1)},

	{vs_ft, ".+", A(AY1 | SYN_BD), 1},
	{vs_ft, "(^[?/])|(\\\\[<>]|\\(\\?[:=!<>#]|\\[\\^?((?:\\\\.?|[^\\]])*)]|\\[|[.^$\\()*+|?]|\\{([0-9]*(,)?[0-9]*)?}?)|\\\\(.)",
		A(SYN_BD, BL1, WH1, GR1, RE1, WH1, YE)},
	{vs_ft, "(\\\\)(.)(?:(-)(?:(\\\\(.))|[^\\\\\\]]))?|[^\\\\[^](-)(?:(\\\\(.))|.)|\
[^\\\\[^]\\^(-)(?:(\\\\(.))|[^\\\\])|\\^(-)[^\\\\]",
		A(GR1 | SYN_BD | SYN_ATT, 1, GR1, AY1, YE, WH1, AY1, YE, WH1, AY1, YE, WH1, AY1, YE, WH1), 2},

	{bar_ft, "^(\".*\").*(\\[[wrf]\\]).*$", A(AY1 | SYN_BD, BL, RE)},
	{bar_ft, "^<(.+)> (?:[^ ]+ )*([0-9]+L) ([0-9]+W) (S[0-9]+) (O[0-9]+) (C[0-9]+)$",
		A(AY1 | SYN_BD, RE1, BL, YE, MA, CY1, YE1)},
	{bar_ft, "^(\".*\").* ([0-9]{1,3}%) (L[0-9]+) (C[0-9]+) (B-?[0-9]+)?.*$",
		A(AY1 | SYN_BD, BL, RE1, BL, YE1, GR)},
	{bar_ft, "^.*$", A(AY1 | SYN_BD)},

	{fuzz_ft, ".+", A(AY1 | SYN_BD)},
	{fuzz_ft, NULL, A(RE1 | SYN_BD), 1, 1},

	{msg_ft, ".+", A(AY1 | SYN_BD)},
};
const int hlslen = LEN(hls);

/* how to highlight text in the reverse direction */
const int conf_hlrev = SYN_BGMK(8);

/* right-to-left characters */
#define CR2L		"ء-يپچژکگی‌-‍؛،»«؟ً-ْٔ"
/* neutral characters */
#define CNEUT		"\x1- !-/:-@[-`{-\x7f"

struct dircontext dctxs[] = {
	{"^[" CR2L "]", -1},
	{"^[a-zA-Z_0-9]", +1},
};
const int dctxlen = LEN(dctxs);

struct dirmark dmarks[] = {
	{"[" CR2L "][" CNEUT CR2L "]*[" CR2L "]", +1, {-1}},
	{"^([ \t]+)?([" CNEUT "]*[^" CR2L "]*[^" CR2L CNEUT "](?:[" CNEUT "]+$)?)", -1, {0, 1, -1}},
	{"[^" CR2L CNEUT "][^" CR2L "]*[^" CR2L CNEUT "](?:[" CNEUT "]+$)?", -1, {-1}},
};
const int dmarkslen = LEN(dmarks);

struct placeholder _ph[2] = {
	{{0x0,0x1f}, "^", 1, 1},
	{{0x200c,0x200d}, "-", 1, 3},
};
struct placeholder *ph = _ph;
int phlen = LEN(_ph);

char **conf_kmap(int id)
{
	return kmaps[id];
}

int conf_kmapfind(char *name)
{
	for (int i = 0; i < LEN(kmaps); i++)
		if (name && kmaps[i][0] && !strcmp(name, kmaps[i][0]))
			return i;
	return 0;
}

char *conf_digraph(int c1, int c2)
{
	for (int i = 0; i < LEN(digraphs); i++)
		if (digraphs[i][0][0] == c1 && digraphs[i][0][1] == c2)
			return digraphs[i][1];
	return NULL;
}
