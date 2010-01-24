# Uses references to automatically close files when the handle
# can no longer be accessed.
#
# e.g. bio copy [autoopen infile] [autoopen outfile w]; collect
#
proc autoopen {filename {mode r}} {
	set ref [ref [open $filename $mode] aio lambdaFinalizer]
	rename [getref $ref] $ref
	return $ref
}

# Hardly needed
proc filecopy {read write} {
	bio copy [autoopen $read] [autoopen $write w]
	collect
}

proc makeFile {contents name} {
	set f [open $name w]
	puts $f $contents
	close $f
}

catch {
	# Tcl-only things
	info tclversion
	proc errorInfo {msg} {
		return $::errorInfo
	}
}

proc section {name} {
	puts "-- $name ----------------"
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
		puts "Expected: '$expected'"
		puts "Got     : '$result'"
		incr ::testresults(numfail)
		lappend ::testresults(failed) [list $id $descr $script $expected $result]
	}
}

proc testreport {} {
	puts "----------------------------------------------------------------------"
	puts "FAILED: $::testresults(numfail)"
	foreach failed $::testresults(failed) {
		foreach {id descr script expected result} $failed {}
		puts "\t$id"
	}
	puts "PASSED: $::testresults(numpass)"
	puts "----------------------------------------------------------------------\n"
}

proc testerror {} {
	error "deliberate error"
}

puts [string repeat = 40]
puts $argv0
puts [string repeat = 40]

set ::testquiet [info exists ::env(testquiet)]
