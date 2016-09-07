#!/usr/bin/env tclsh

# Generate UTF-8 case mapping tables
#
# (c) 2010 Steve Bennett <steveb@workware.net.au>
#
# See LICENCE for licence details.
#/

# Parse the unicode data from: http://unicode.org/Public/UNIDATA/UnicodeData.txt
# to generate case mapping and display width tables
set map(lower) {}
set map(upper) {}
set map(title) {}
set map(combining) {}

set USAGE "Usage: parse-unidata.tcl \[-width\] UnicodeData.txt"

set do_width 0
foreach arg $argv {
	if {$arg eq "-width"} {
		incr do_width
	} else {
		if {[info exists filename]} {
			puts stderr $USAGE
			exit 1
		}
		set filename $arg
	}
}
if {![info exists filename]} {
	puts stderr $USAGE
	exit 1
}

# Why isn't this available in UnicodeData.txt?
set map(wide) {
	0x1100 0x115f 0x2329 0x232a 0x2e80 0x2e99 0x2e9b 0x2ef3
	0x2f00 0x2fd5 0x2ff0 0x2ffb 0x3000 0x303e 0x3041 0x3096
	0x3099 0x30ff 0x3105 0x312d 0x3131 0x318e 0x3190 0x31ba
	0x31c0 0x31e3 0x31f0 0x321e 0x3220 0x3247 0x3250 0x4dbf
	0x4e00 0xa48c 0xa490 0xa4c6 0xa960 0xa97c 0xac00 0xd7a3
	0xf900 0xfaff 0xfe10 0xfe19 0xfe30 0xfe52 0xfe54 0xfe66
	0xfe68 0xfe6b 0xff01 0xffe6 0x1b000 0x1b001 0x1f200 0x1f202
	0x1f210 0x1f23a 0x1f240 0x1f248 0x1f250 0x1f251 0x20000 0x3fffd
}

set f [open $filename]
while {[gets $f buf] >= 0} {
	set title ""
	set lower ""
	set upper ""
	foreach {code name class x x x x x x x x x upper lower title} [split $buf ";"] break
	set codex [string tolower 0x$code]
	if {[string match M* $class]} {
		if {![info exists combining]} {
			set combining $codex
		}
		continue
	} elseif {[info exists combining]} {
		lappend map(combining) $combining $codex
		unset combining
	}
	if {$codex <= 0x7f} {
		continue
	}
	if {$codex > 0xffff} {
		break
	}
	if {![string match L* $class]} {
		continue
	}
	if {$upper ne ""} {
		lappend map(upper) $codex [string tolower 0x$upper]
	}
	if {$lower ne ""} {
		lappend map(lower) $codex [string tolower 0x$lower]
	}
	if {$title ne "" && $title ne $upper} {
		if {$title eq $code} {
			set title 0
		}
		lappend map(title) $codex [string tolower 0x$title]
	}
}
close $f

proc output-int-pairs {list} {
	set n 0
	foreach {v1 v2} $list {
		puts -nonewline "\t{ $v1, $v2 },"
		if {[incr n] % 4 == 0} {
			puts ""
		}
	}
	if {$n % 4} {
		puts ""
	}
}

foreach type {upper lower title} {
	puts "static const struct casemap unicode_case_mapping_$type\[\] = \{"
	output-int-pairs $map($type)
	puts "\};\n"
}

foreach type {combining wide} {
	puts "static const struct utf8range unicode_range_$type\[\] = \{"
	if {$do_width} {
		output-int-pairs $map($type)
	} else {
		# Just produce empty width tables in this case
		output-int-pairs {}
	}
	puts "\};\n"
}
