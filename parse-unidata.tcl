#!/usr/bin/env tclsh

# Generate UTF-8 case mapping tables
#
# (c) 2010 Steve Bennett <steveb@workware.net.au>
#
# See LICENCE for licence details.
#/

# Parse the unicode data from: http://unicode.org/Public/UNIDATA/UnicodeData.txt
# and http://unicode.org/Public/UNIDATA/EastAsianWidth.txt
# to generate case mapping and display width tables
set map(lower) {}
set map(upper) {}
set map(title) {}
set map(combining) {}
set map(wide) {}

set USAGE "Usage: parse-unidata.tcl \[-width\] UnicodeData.txt \[EastAsianWidth.txt\]"
set do_width 0

if {[lindex $argv 0] eq "-width"} {
	set do_width 1
	set argv [lrange $argv 1 end]
}

if {[llength $argv] ni {1 2}} {
	puts stderr $USAGE
	exit 1
}

lassign $argv unicodefile widthfile

set f [open $unicodefile]
while {[gets $f buf] >= 0} {
    # remove carriage returns (inserted by git??) not filtered out by bootstrap shell jimsh0.
    regsub {\r} $buf "" buf
	set title ""
	set lower ""
	set upper ""
	lassign [split $buf ";"] code name class x x x x x x x x x upper lower title
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

# Merges adjacent ranges in a list of ranges (lower upper lower upper ...)
proc combine-adjacent-ranges {list} {
	set newlist {}
	foreach {lower upper} $list {
		if {[info exists prev_upper]} {
			if {$lower == $prev_upper + 1} {
				# combine these
				set prev_upper $upper
				continue
			} else {
				# can't combine
				lappend newlist $prev_lower $prev_upper
			}
		}
		set prev_lower $lower
		set prev_upper $upper
	}
	# Now add the last range
	lappend newlist $prev_lower $prev_upper
	return $newlist
}

foreach type {upper lower title} {
	puts "static const struct casemap unicode_case_mapping_$type\[\] = \{"
	output-int-pairs $map($type)
	puts "\};\n"
}

if {$do_width} {
	set f [open $widthfile]
	while {[gets $f buf] >= 0} {
		if {[regexp {^([0-9A-Fa-f.]+);W} $buf -> range]} {
			set range [string tolower $range]
			lassign [split $range .] lower - upper
			if {$upper eq ""} {
				set upper $lower
			}
			lappend map(wide) 0x$lower 0x$upper
		}
	}
	close $f
}

foreach type {combining wide} {
	puts "static const struct utf8range unicode_range_$type\[\] = \{"
	if {$do_width} {
		output-int-pairs [combine-adjacent-ranges $map($type)]
	} else {
		# Just produce empty width tables in this case
		output-int-pairs {}
	}
	puts "\};\n"
}
