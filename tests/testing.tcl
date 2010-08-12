proc makeFile {contents name} {
	set f [open $name w]
	puts $f $contents
	close $f
}

proc info_source {script} {
	join [info source $script] :
}

catch {
	# Tcl-only things
	info tclversion
	proc errorInfo {msg} {
		return $::errorInfo
	}
	proc info_source {script} {
		return ""
	}
}

proc section {name} {
	if {!$::testquiet} {
		puts "-- $name ----------------"
	}
}

set testresults(numfail) 0
set testresults(numpass) 0
set testresults(failed) {}

proc test {id descr script expected} {
	if {!$::testquiet} {
		puts -nonewline "$id "
	}
	set rc [catch {uplevel 1 $script} result]
	# Note that rc=2 is return
	if {($rc == 0 || $rc == 2) && $result eq $expected} {
		if {!$::testquiet} {
			puts "OK  $descr"
		}
		incr ::testresults(numpass)
	} else {
		if {$::testquiet} {
			puts -nonewline "$id "
		}
		puts "ERR $descr"
		puts "At      : [info_source $script]"
		puts "Expected: '$expected'"
		puts "Got     : '$result'"
		incr ::testresults(numfail)
		lappend ::testresults(failed) [list $id $descr $script $expected $result]
	}
}

proc testreport {} {
	if {!$::testquiet || $::testresults(numfail)} {
		puts "----------------------------------------------------------------------"
		puts "FAILED: $::testresults(numfail)"
		foreach failed $::testresults(failed) {
			foreach {id descr script expected result} $failed {}
			puts "\t[info_source $script]\t$id"
		}
		puts "PASSED: $::testresults(numpass)"
		puts "----------------------------------------------------------------------\n"
	}
	if {$::testresults(numfail)} {
		exit 1
	}
}

proc testerror {} {
	error "deliberate error"
}

incr testquiet [info exists ::env(testquiet)]
if {[lindex $argv 0] eq "-quiet"} {
	incr testquiet
}

if {!$testquiet} {
	puts [string repeat = 40]
	puts $argv0
	puts [string repeat = 40]
}
