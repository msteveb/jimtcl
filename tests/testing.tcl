# Uses references to automatically close files when the handle
# can no longer be accessed.
#
# e.g. bio copy [autoopen infile] [autoopen outfile w]; collect
#
proc autoopen {filename {mode r}} {
	set ref [ref [aio.open $filename $mode] aio lambdaFinalizer]
	rename [getref $ref] $ref
	return $ref
}

# And make autoopen the standard open
rename open ""
rename autoopen open

# Hardly needed
proc filecopy {read write} {
	bio copy [open $read] [open $write w]
}

proc section {name} {
	puts "-- $name ----------------"
}

array set testresults {numfail 0 numpass 0 failed {}}

proc test {id descr script expected} {
    puts -nonewline "$id "
    set rc [catch {uplevel 1 $script} result]
	# Note that rc=2 is return
    if {($rc == 0 || $rc == 2) && $result eq $expected} {
		puts "OK  $descr"
		incr ::testresults(numpass)
    } else {
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
