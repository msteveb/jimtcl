# tcltest compatibilty/wrapper/extension

# Common code
set testinfo(verbose) 0
set testinfo(numpass) 0
set testinfo(stoponerror) 0
set testinfo(numfail) 0
set testinfo(numskip) 0
set testinfo(numtests) 0
set testinfo(reported) 0
set testinfo(failed) {}

if {[lsearch $argv "-verbose"] >= 0 || [info exists env(testverbose)]} {
	incr testinfo(verbose)
}
if {[lsearch $argv "-stoponerror"] >= 0 || [info exists env(stoponerror)]} {
	incr testinfo(stoponerror)
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
	if {$type eq "package"} {
		if {[catch {package require $what}]} {
			skiptest " (package $what)"
		}
		return
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

# Add some search paths for packages
if {[exists argv0]} {
	# The directory containing the original script
	lappend auto_path [file dirname $argv0]
}
# The directory containing the jimsh executable
lappend auto_path [file dirname [info nameofexecutable]]

# For Jim, this is reasonable compatible tcltest
proc makeFile {contents name {dir {}}} {
	if {$dir eq ""} {
		set filename $name
	} else {
		set filename $dir/$name
	}
	set f [open $filename w]
	puts $f $contents
	close $f
	return $filename
}

proc makeDirectory {name} {
	file mkdir $name
	return $name
}

proc temporaryDirectory {} {{dir {}}} {
	if {$dir eq ""} {
		set dir [file join [env TMPDIR /tmp] [format "tcltmp-%04x" [rand 65536]]]
		file mkdir $dir
	}
	return $dir
}

proc removeFile {args} {
	file delete -force {*}$args
}

proc removeDirectory {name} {
	file delete -force $name
}

# In case tclcompat is not selected
if {![exists -proc puts]} {
	proc puts {{-nonewline {}} {chan stdout} msg} {
		if {${-nonewline} ni {-nonewline {}}} {
			${-nonewline} puts $msg
		} else {
			$chan puts {*}${-nonewline} $msg
		}
	}
	proc close {chan args} {
		$chan close {*}$args
	}
	proc fileevent {args} {
		{*}$args
	}
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

proc testConstraint {constraint {bool {}}} {
	if {$bool eq ""} {
		if {[info exists ::tcltest::testConstraints($constraint)]} {
			return $::tcltest::testConstraints($constraint)
		}
		return -code error "unknown constraint: $constraint"
		return 1
	} else {
		set ::tcltest::testConstraints($constraint) $bool
	}
}

testConstraint {utf8} [expr {[string length "\xc2\xb5"] == 1}]
testConstraint {references} [expr {[info commands getref] ne ""}]
testConstraint {jim} 1
testConstraint {tcl} 0

proc bytestring {x} {
	return $x
}

# Note: We don't support -output or -errorOutput yet
proc test {id descr args} {
	set a [dict create -returnCodes {ok return} -match exact -result {} -constraints {} -body {} -setup {} -cleanup {}]
	if {[lindex $args 0] ni [dict keys $a]} {
		if {[llength $args] == 2} {
			lassign $args body result constraints
		} elseif {[llength $args] == 3} {
			lassign $args constraints body result
		} else {
			return -code error "$id: Wrong syntax for tcltest::test v1"
		}
		tailcall test $id $descr -body $body -result $result -constraints $constraints
	}
	# tcltest::test v2 syntax
	array set a $args

	incr ::testinfo(numtests)
	if {$::testinfo(verbose)} {
		puts -nonewline "$id "
	}

	foreach c $a(-constraints) {
		if {![testConstraint $c]} {
			incr ::testinfo(numskip)
			if {$::testinfo(verbose)} {
				puts "SKIP $descr"
			}
			return
		}
	}

	if {[catch {uplevel 1 $a(-setup)} msg]} {
		if {$::testinfo(verbose)} {
			puts "-setup failed: $msg"
		}
	}
	set rc [catch {uplevel 1 $a(-body)} result opts]
	if {[catch {uplevel 1 $a(-cleanup)} msg]} {
		if {$::testinfo(verbose)} {
			puts "-cleanup failed: $msg"
		}
	}

	if {[info return $rc] ni $a(-returnCodes) && $rc ni $a(-returnCodes)} {
		set ok 0
		set expected "rc=$a(-returnCodes) result=$a(-result)"
		set result "rc=[info return $rc] result=$result"
	} else {
		if {$a(-match) eq "exact"} {
			set ok [string equal $a(-result) $result]
		} elseif {$a(-match) eq "glob"} {
			set ok [string match $a(-result) $result]
		} elseif {$a(-match) eq "regexp"} {
			set ok [regexp $a(-result) $result]
		} else {
			return -code error "$id: unknown match type: $a(-match)"
		}
		set expected $a(-result)
	}

	if {$ok} {
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
	if {$rc in {0 2}} {
		set source [script_source $a(-body)]
	} else {
		set source [error_source]
	}
	puts "Expected: '$expected'"
	puts "Got     : '$result'"
	puts ""
	incr ::testinfo(numfail)
	lappend ::testinfo(failed) [list $id $descr $source $expected $result]
	if {$::testinfo(stoponerror)} {
		exit 1
	}
}

proc ::tcltest::cleanupTests {} {
	file delete [temporaryDirectory]
	tailcall testreport
}

proc testreport {} {
	if {$::testinfo(reported)} {
		return
	}
	incr ::testinfo(reported)

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
