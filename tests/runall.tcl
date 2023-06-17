# Run all tests in the current directory
# 
# Tests are run in a sub-interpreter (if possible) to avoid
# interactions between tests.

lappend auto_path .

set testdir [file dirname [info script]]

# In case interp is a module
catch {package require interp}

if {[info commands interp] eq ""} {
	set rc 0
	foreach script [lsort [glob $testdir/*.test]] {
		if {[catch {
			exec [info nameofexecutable] $script >@stdout 2>@stderr
		} msg opts]} {
			puts "Failed: $script"
			set rc 1
		}
	}
	exit $rc
} else {
	array set total {pass 0 fail 0 skip 0 tests 0}
	foreach script [lsort [glob $testdir/*.test]] {
		set ::argv0 $script

		if {[file tail $script] in {signal.test event.test exec2.test}} {
			# special case, can't run these in a child interpeter
			catch -exit {
				source $script
			}
			foreach var {pass fail skip tests} {
				incr total($var) $testinfo(num$var)
			}
		} else {
			set i [interp]

			foreach var {argv0 auto_path} {
				$i eval [list set $var [set ::$var]]
			}

			# Run the test
			catch -exit [list $i eval [list source $script]] msg opts
			if {[info returncode $opts(-code)] eq "error"} {
				puts [format "%16s:   --- error ($msg)" $script]
				incr total(fail)
			} elseif {[info return $opts(-code)] eq "exit"} {
				# if the test explicitly called exit 98 or 99,
				# it must be from a child process via os.fork, so
				# silently exit with that return code
				if {$msg in {98 99}} {
					exit $msg
				}
			}

			# Extract the counts
			foreach var {pass fail skip tests} {
				catch {
					incr total($var) [$i eval "set testinfo(num$var)"]
				}
			}
			$i delete
		}

		stdout flush
	}
	puts [string repeat = 73]
	puts [format "%16s: Total %5d   Passed %5d  Skipped %5d  Failed %5d" \
			Totals $total(tests) $total(pass) $total(skip) $total(fail)]

	if {$total(fail)} {
		exit 1
	}
}
