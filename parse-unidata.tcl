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

set f [open [lindex $argv 0]]
while {[gets $f buf] >= 0} {
	foreach {code name class x x x x x x x x x upper lower} [split $buf ";"] break
	set code [string tolower 0x$code]
	if {$code <= 0x7f} {
		continue
	}
	if {$code > 0xffff} {
		break
	}
	if {![string match L* $class]} {
		continue
	}
	if {$upper ne ""} {
		lappend map(upper) $code [string tolower 0x$upper]
	}
	if {$lower ne ""} {
		lappend map(lower) $code [string tolower 0x$lower]
	}
}
close $f

foreach type {upper lower} {
	puts "static const struct casemap unicode_case_mapping_$type\[\] = \{"
	foreach {code alt} $map($type) {
		puts "\t{ $code, $alt },"
	}
	puts "\};\n"
}
