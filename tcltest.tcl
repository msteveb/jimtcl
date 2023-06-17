# tcltest compatibilty/wrapper/extension

# Common code
set testinfo(verbose) 0
set testinfo(numpass) 0
set testinfo(stoponerror) 0
set testinfo(template) 0
set testinfo(numfail) 0
set testinfo(numskip) 0
set testinfo(numtests) 0
set testinfo(reported) 0
set testinfo(failed) {}
set testinfo(source) [file tail $::argv0]

# -verbose or $testverbose show OK/ERR of individual tests
if {[lsearch $argv "-verbose"] >= 0 || [info exists env(testverbose)]} {
	incr testinfo(verbose)
}
# -template causes failed tests to output a template test that would succeed
if {[lsearch $argv "-template"] >= 0} {
	incr testinfo(template)
}
# -stoponerror or $stoponerror stops on the first failed test
if {[lsearch $argv "-stoponerror"] >= 0 || [info exists env(stoponerror)]} {
	incr testinfo(stoponerror)
}

proc needscmdcheck {what {packages {}}} {
	# Does it exist already?
	if {[info commands $what] ne ""} {
		return 1
	}
	if {$packages eq ""} {
		# e.g. exec command is in exec package
		set packages $what
	}
	foreach p $packages {
		catch {package require $p}
	}
	if {[info commands $what] ne ""} {
		return 1
	}
	return 0
}

# Verifies the constraint/need
# Returns 1 if the check passes or 0 if not
proc needcheck {type what args} {
	# Returns 1 if the check passed or 0 if not
	if {$type eq "constraint"} {
		if {[info exists ::tcltest::testConstraints($what)] && $::tcltest::testConstraints($what)} {
			return 1
		}
		return 0
	}
	if {$type eq "cmd"} {
		lassign $what cmd subcmd
		if {![needscmdcheck $cmd $args]} {
			return 0
		}
		if {$subcmd ne ""} {
			if {[testConstraint jim]} {
				if {$subcmd ni [$cmd -commands]} {
					return 0
				}
			} else {
				if {[catch {$cmd $subcmd}]} {
					return 0
				}
			}
		}
		return 1
	}
	if {$type eq "package"} {
		if {[catch {package require $what}]} {
			return 0
		}
		return 1
	}
	if {$type eq "expr"} {
		return [uplevel #0 [list expr [lindex $args 0]]]
	}
	if {$type eq "eval"} {
		try {
			uplevel #0 [lindex $args 0]
			return 1
		} on error msg {
			return 0
		}
	}
	error "Unknown needs type: $type"
}

# needs skips all tests in the file if the requirement isn't met
# constrains sets a constraint to 1 or 0 based on if the requirement is met.
#
# needs|constraint cmd {cmd ?subcmd?} ?packages?
# 
# Checks that the command 'cmd' (and possibly 'cmd subcmd') exists
# If necessary, loads the given packages
# If used as a constraint, the constraint name is $cmd or $cmd-$subcmd
# 
# needs constraint name
# 
# Checks that the given constraint is set and is met (true)
# If the constraint hasn't been set, this check fails (returns false)
# 
# needs|constraint expr name <expression>
#
# Checks that the expression evaluates to true.
# If used as a constraint, the constraint name is $name
#
# needs|constraint eval name <script>
#
# Checks that the script evaluated at global scope does not produce an error.
# If used as a constraint, the constraint name is $name
#
# needs|constraint package name ?packages?
#
# Checks that the given package is/can be loaded.
# If necessary, loads the given packages first
# If used as a constraint, the constraint name is package-$name

proc constraint {type what args} {
	# XXX constraint constraint doesn't make any sense
	set ok [needcheck $type $what {*}$args]
	if {$type eq "package"} {
		testConstraint package-$what $ok
	} else {
		testConstraint [join $what -] $ok
	}
}

proc needs {type what args} {
	if {![needcheck $type $what {*}$args]} {
		skiptest "  ($type $what)"
	}
}

proc skiptest {{msg {}}} {
	#puts [errorInfo $msg [stacktrace]]
	puts [format "%16s:   --- skipped$msg" $::testinfo(source)]
	exit 0
}

# Takes a stacktrace and applies [file tail] to the filenames.
# This allows stacktrace tests to be run from a directory other than the source directory.
# Also convert proc name ::a into a for compatibility between Tcl and Jim
proc basename-stacktrace {stacktrace} {
	set result {}
	foreach {p f l cmd} $stacktrace {
		if {[string match *tcltest-* $f]} {
			#break
		}
		if {$p eq "::tcltest::RunTest"} {
			set p test
		} elseif {[string match ::* $p]} {
			set p [string range $p 2 end]
		}
		set cmd [string map [list \n \\n] $cmd]
		if {[string length $cmd] > 20} {
			set cmd [string range $cmd 0 20]...
		}
		lappend result $p [file tail $f] $l $cmd
	}
	return $result
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
	proc stacktrace {{skip 0} {last 0}} {
		set frames {}
		incr skip
		for {set level $skip} {$level < [info frame] - $last} {incr level} {
			set frame [info frame -$level]
			puts $frame
			if {[dict get $frame type] ne "source"} {
				continue
			}
			if {[dict exists $frame proc]} {
				set proc [dict get $frame proc]
			} else {
				set proc ""
			}
			lappend frames $proc [dict get $frame file] [dict get $frame line] [dict get $frame cmd]
		}
		return $frames
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
		puts "$f:$l:Error test failure"
		return \t$f:$l
	}
}

proc error_source {} {
	lassign [info stacktrace] p f l
	if {$f ne ""} {
		puts "$f:$l:Error test failure"
		return \t$f:$l
	}
}

proc package-or-skip {name} {
	if {[catch {
		package require $name
	}]} {
		puts [format "%16s:   --- skipped" $::testinfo(source)]
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

# Takes a list of {filename line} and returns {basename line}
proc basename-source {list} {
	list [file tail [lindex $list 0]] [lindex $list 1]
}

# Note: We don't support -output or -errorOutput yet
proc test {id descr args} {
	set default [dict create -returnCodes {ok return} -match exact -result {} -constraints {} -body {} -setup {} -cleanup {}]
	set a $default
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
		set expected "rc=[list $a(-returnCodes)] result=[list $a(-result)]"
		set actual "rc=[info return $rc] result=[list $result]"
		# Now for the template, update -returnCodes
		set a(-returnCodes) [info return $rc]
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
		set actual [list $result]
		set expected [list $a(-result)]
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
	puts "Expected: $expected"
	puts "Got     : $actual"
	puts ""
	if {$::testinfo(template)} {
		# We can't really do -match glob|regexp so
		# just store the result as-is for -match exact
		set a(-result) $result

		set template [list test $id $descr]
		foreach key {-constraints -setup -body -returnCodes -match -result -cleanup} {
			if {$a($key) ne $default($key)} {
				lappend template $key $a($key)
			}
		}
		puts "### template"
		puts $template\n
	}
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
		puts -nonewline "\n$::testinfo(source)"
	} else {
		puts -nonewline [format "%16s" $::testinfo(source)]
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
