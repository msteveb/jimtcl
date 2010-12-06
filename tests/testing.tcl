# Common code
set testinfo(verbose) 0
set testinfo(numpass) 0
set testinfo(numfail) 0
set testinfo(numskip) 0
set testinfo(numtests) 0
set testinfo(failed) {}

set testdir [file dirname [info script]]

if {[lsearch $argv "-verbose"] >= 0 || [info exists env(testverbose)]} {
	incr testinfo(verbose)
}

proc needs {type what {packages {}}} {
	if {$type eq "constraint"} {
		if {![info exists ::tcltest::testConstraints($what)]} {
			set ::tcltest::testConstraints($what) 0
		}
		if {![set ::tcltest::testConstraints($what)]} {
			skiptest " (constraint $what)"
		}
		return
	}
	if {$type eq "cmd"} {
		# Does it exist already?
		if {[info commands $what] ne ""} {
			return
		}
		if {$packages eq ""} {
			# e.g. exec command is in exec package
			set packages $what
		}
		foreach p $packages {
			catch {package require $p}
		}
		if {[info commands $what] ne ""} {
			return
		}
		skiptest " (command $what)"
	}
	error "Unknown needs type: $type"
}

proc skiptest {{msg {}}} {
	puts [format "%16s:   --- skipped$msg" $::argv0]
	exit 0
}

# If tcl, just use tcltest
if {[catch {info version}]} {
	package require Tcl 8.5
	package require tcltest 2.1
	namespace import tcltest::*

	if {$testinfo(verbose)} {
		configure -verbose bps
	}
	testConstraint utf8 1
	testConstraint tcl 1
	proc testreport {} {
		::tcltest::cleanupTests
	}
	return
}

# For Jim, this is reasonable compatible tcltest
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
		puts [format "%16s:   --- skipped" $::argv0]
		exit 0
	}
}

proc testConstraint {constraint bool} {
	set ::tcltest::testConstraints($constraint) $bool
}

testConstraint {utf8} [expr {[string length "\xc2\xb5"] == 1}]
testConstraint {references} [expr {[info commands ref] ne ""}]
testConstraint {jim} 1

proc bytestring {x} {
	return $x
}

proc test {id descr script {constraints {}} expected} {
	incr ::testinfo(numtests)
	if {$::testinfo(verbose)} {
		puts -nonewline "$id "
	}

	foreach c $constraints {
		if {![info exists ::tcltest::testConstraints($c)]} {
			incr ::testinfo(numskip)
			if {$::testinfo(verbose)} {
				puts "SKIP"
			}
			return
		}
	}
	set rc [catch {uplevel 1 $script} result]

	# Note that rc=2 is return
	if {($rc == 0 || $rc == 2) && $result eq $expected} {
		if {$::testinfo(verbose)} {
			puts "OK  $descr"
		}
		incr ::testinfo(numpass)
		return
	}

	if {!$::testinfo(verbose)} {
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
	incr ::testinfo(numfail)
	lappend ::testinfo(failed) [list $id $descr $source $expected $result]
}

proc testreport {} {
	if {$::testinfo(verbose)} {
		puts -nonewline "\n$::argv0"
	} else {
		puts -nonewline [format "%16s" $::argv0]
	}
	puts [format ": Total %5d   Passed %5d  Skipped %5d  Failed %5d" \
		$::testinfo(numtests) $::testinfo(numpass) $::testinfo(numskip) $::testinfo(numfail)]
	if {$::testinfo(numfail)} {
		puts [string repeat - 60]
		puts "FAILED: $::testinfo(numfail)"
		foreach failed $::testinfo(failed) {
			foreach {id descr source expected result} $failed {}
			puts "$source\t$id"
		}
		puts [string repeat - 60]
	}
	if {$::testinfo(numfail)} {
		exit 1
	}
}

proc testerror {} {
	error "deliberate error"
}

if {$testinfo(verbose)} {
	puts "==== $argv0 ===="
}
