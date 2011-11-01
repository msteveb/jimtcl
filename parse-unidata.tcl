#!/usr/bin/env tclsh

# Generate UTF-8 case mapping tables
#
# (c) 2010 Steve Bennett <steveb@workware.net.au>
#
# See LICENCE for licence details.
#/

# Parse the unicode data from: http://unicode.org/Public/UNIDATA/UnicodeData.txt
# to generate case mapping tables
set map(lower) {}
set map(upper) {}
set map(title) {}

set f [open [lindex $argv 0]]
while {[gets $f buf] >= 0} {
	set title ""
	set lower ""
	set upper ""
	foreach {code name class x x x x x x x x x upper lower title} [split $buf ";"] break
	set codex [string tolower 0x$code]
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

foreach type {upper lower title} {
	puts "static const struct casemap unicode_case_mapping_$type\[\] = \{"
	foreach {code alt} $map($type) {
		puts "\t{ $code, $alt },"
	}
	puts "\};\n"
}
