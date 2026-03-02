/* Copyright (C) 2007 gnombat@users.sourceforge.net */
/* License: http://shjs.sourceforge.net/doc/license.html */
if (! this.sh_languages) {
  this.sh_languages = {};
}
sh_languages['tcl'] = [
  [
    {
      'next': 1,
      'regex': /#/g,
      'style': 'sh_comment'
    },
    {
      'regex': /\b[+-]?(?:(?:0x[A-Fa-f0-9]+)|(?:(?:[\d]*\.)?[\d]+(?:[eE][+-]?[\d]+)?))u?(?:(?:int(?:8|16|32|64))|L)?\b/g,
      'style': 'sh_number'
    },
    {
      'next': 2,
      'regex': /"/g,
      'style': 'sh_string'
    },
    {
      'next': 3,
      'regex': /'/g,
      'style': 'sh_string'
    },
    {
      'regex': /~|!|%|\^|\*|\(|\)|-|\+|=|\[|\]|\\|:|;|,|\.|\/|\?|&|<|>|\|/g,
      'style': 'sh_symbol'
    },
    {
      'regex': /\{|\}/g,
      'style': 'sh_cbracket'
    },
    {
      'regex': /\b(?:proc|global|upvar|if|then|else|elseif|for|foreach|break|continue|while|set|eval|case|in|switch|default|exit|error|proc|return|uplevel|loop|for_array_keys|for_recursive_glob|for_file|unwind_protect|expr|catch|namespace|rename|variable|method|itcl_class|public|protected|append|binary|format|re_syntax|regexp|regsub|scan|string|subst|concat|join|lappend|lindex|list|llength|lrange|lreplace|lsearch|lset|lsort|split|expr|incr|close|eof|fblocked|fconfigure|fcopy|file|fileevent|flush|gets|open|puts|read|seek|socket|tell|load|loadTk|package|pgk::create|pgk_mkIndex|source|bgerror|history|info|interp|memory|unknown|enconding|http|msgcat|cd|clock|exec|exit|glob|pid|pwd|time|dde|registry|resource)\b/g,
      'style': 'sh_keyword'
    },
    {
      'regex': /\$[A-Za-z0-9_]+/g,
      'style': 'sh_variable'
    }
  ],
  [
    {
      'exit': true,
      'regex': /$/g
    }
  ],
  [
    {
      'exit': true,
      'regex': /"/g,
      'style': 'sh_string'
    },
    {
      'regex': /\\./g,
      'style': 'sh_specialchar'
    }
  ],
  [
    {
      'exit': true,
      'regex': /'/g,
      'style': 'sh_string'
    },
    {
      'regex': /\\./g,
      'style': 'sh_specialchar'
    }
  ]
];
sh_languages['unix'] = [
  [
    {
      'regex': /\$ .*/g,
      'style': 'sh_comment'
    }
  ]
];
sh_languages['autosetup'] = [
  [
    {
      'next': 1,
      'regex': /#/g,
      'style': 'sh_comment'
    },
    {
      'regex': /\b[+-]?(?:(?:0x[A-Fa-f0-9]+)|(?:(?:[\d]*\.)?[\d]+(?:[eE][+-]?[\d]+)?))u?(?:(?:int(?:8|16|32|64))|L)?\b/g,
      'style': 'sh_number'
    },
    {
      'next': 2,
      'regex': /"/g,
      'style': 'sh_string'
    },
    {
      'regex': /~|!|%|\^|\*|\(|\)|-|\+|=|\[|\]|\\|:|;|,|\.|\/|\?|&|<|>|\|/g,
      'style': 'sh_symbol'
    },
    {
      'regex': /\{|\}/g,
      'style': 'sh_cbracket'
    },
    {
      'regex': /\b(?:cc-[-a-z]*|define[-a-z]*|get-define|user-error|options|use|opt-[a-z]*|msg-[-a-z]*|make-[-a-z]*|have-[-a-z]*)\s/g,
      'style': 'sh_function'
    },
    {
      'regex': /\b(?:proc|global|upvar|if|then|else|elseif|for|foreach|break|continue|while|set|eval|case|in|switch|default|exit|error|proc|return|uplevel|loop|expr|catch|namespace|rename|variable|method|public|protected|append|binary|format|re_syntax|regexp|regsub|scan|string|subst|concat|join|lappend|lindex|list|llength|lrange|lreplace|lsearch|lset|lsort|split|expr|incr|close|eof|fblocked|fconfigure|fcopy|file|fileevent|flush|gets|open|puts|read|seek|socket|tell|load|loadTk|package|pgk::create|pgk_mkIndex|source|bgerror|history|info|interp|memory|unknown|enconding|http|msgcat|cd|clock|exec|exit|glob|pid|pwd|time|dde|registry|resource)\b/g,
      'style': 'sh_keyword'
    },
    {
      'regex': /\$[A-Za-z0-9_]+/g,
      'style': 'sh_variable'
    }
  ],
  [
    {
      'exit': true,
      'regex': /$/g
    }
  ],
  [
    {
      'exit': true,
      'regex': /"/g,
      'style': 'sh_string'
    },
    {
      'regex': /\\./g,
      'style': 'sh_specialchar'
    }
  ],
];
