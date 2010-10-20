proc makeFile {contents name} {
	set f [open $name w]
	puts $f $contents
	close $f
}

proc script_source {script} {
	lassign [info source $script] f l
	if {$f ne ""} {
		puts "At      : $f:$l"
		return \t$f:$l
	}
}

proc error_source {} {
	lassign [info stacktrace] p f l
	if {$f ne ""} {
		puts "At      : $f:$l"
		return \t$f:$l
	}
}

proc package-or-skip {name} {
	if {[catch {
		package require $name
	}]} {
		puts "   --- skipped"
		exit 0
	}
}

set test(utf8) 0
if {[string length "\xc2\xb5"] == 1} {
	set test(utf8) 1
}
proc bytestring {x} {
	return $x
}

catch {
	# Tcl-only things
	info tclversion
	proc errorInfo {msg} {
		return $::errorInfo
	}
	proc error_source {} {
	}
	proc script_source {script} {
	}
	set test(utf8) 1
	rename bytestring ""
	package require tcltest
	interp alias {} bytestring {} ::tcltest::bytestring
}

proc ifutf8 {code} {
	if {$::test(utf8)} {
		uplevel 1 $code
	}
}

proc section {name} {
	if {!$::test(quiet)} {
		puts "-- $name ----------------"
	}
}

set test(numfail) 0
set test(numpass) 0
set test(failed) {}

proc test {id descr script expected} {
	if {!$::test(quiet)} {
		puts -nonewline "$id "
	}

	set rc [catch {uplevel 1 $script} result]

	# Note that rc=2 is return
	if {($rc == 0 || $rc == 2) && $result eq $expected} {
		if {!$::test(quiet)} {
			puts "OK  $descr"
		}
		incr ::test(numpass)
	} else {
		if {$::test(quiet)} {
			puts -nonewline "$id "
		}
		puts "ERR $descr"
		if {$rc == 0} {
			set source [script_source $script]
		} else {
			set source [error_source]
		}
		puts "Expected: '$expected'"
		puts "Got     : '$result'"
		incr ::test(numfail)
		lappend ::test(failed) [list $id $descr $source $expected $result]
	}
}

proc testreport {} {
	if {!$::test(quiet) || $::test(numfail)} {
		puts "----------------------------------------------------------------------"
		puts "FAILED: $::test(numfail)"
		foreach failed $::test(failed) {
			foreach {id descr source expected result} $failed {}
			puts "$source\t$id"
		}
		puts "PASSED: $::test(numpass)"
		puts "----------------------------------------------------------------------\n"
	}
	if {$::test(numfail)} {
		exit 1
	}
}

proc testerror {} {
	error "deliberate error"
}

set test(quiet) [info exists ::env(testquiet)]
if {[lindex $argv 0] eq "-quiet"} {
	incr test(quiet)
}

if {!$test(quiet)} {
	puts [string repeat = 40]
	puts $argv0
	puts [string repeat = 40]
}
