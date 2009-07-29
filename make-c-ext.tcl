#!/usr/bin/env tclsh

proc tcl_to_string {str} {
	set result {}
	foreach buf [split $str \n] {
		set trimmed [string trim $buf]
		if {[string match "#*" $trimmed] || $trimmed == ""} {
			continue
		}
		regsub -all {\\} $buf {\\\\} buf 
		regsub -all \" $buf "\\\"" buf
		append result {"} $buf {\n"} \n
	}
	return $result
}

set outfile [lindex $argv 0]
set argv [lrange $argv 1 end]

foreach file $argv {
	if {![string match *.tcl $file]} {
		error "Not a tcl file: $file"
	}
	set tmp [file tail $file]
	set rootname [file rootname $tmp]
	if {0} {
	set outfile jim-$rootname.c
	}
	set f [open $file]
	set str [read $f]
	close $f
	set f [open $outfile w]
	puts $f {#include <jim.h>}
	puts $f "int Jim_${rootname}Init(Jim_Interp *interp)"
	puts $f "{"
	puts $f "\treturn Jim_EvalGlobal(interp, "
	puts -nonewline $f [tcl_to_string $str]
	puts $f ");\t}"
	close $f
}
