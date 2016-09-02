# Run all tests in the current directory
# 
# Tests are run in a sub-interpreter (if possible) to avoid
# interactions between tests.

lappend auto_path .

# In case interp is a module
catch {package require interp}

if {[info commands interp] eq ""} {
	set rc 1
	foreach script [lsort [glob *.test]] {
		if {[catch {
			exec [info nameofexecutable] $script >@stdout 2>@stderr
			set rc 0
		} msg opts]} {
			puts "Failed: $script"
		}
	}
	exit $rc
} else {
	array set total {pass 0 fail 0 skip 0 tests 0}
	foreach script [lsort [glob *.test]] {
		set ::argv0 $script

		set i [interp]

		foreach var {argv0 auto_path} {
			$i eval [list set $var [set ::$var]]
		}

		# Run the test
		catch -exit {$i eval source $script} msg opts
		if {[info returncode $opts(-code)] eq "error"} {
			puts [format "%16s:   --- error ($msg)" $script]
			incr total(fail)
		}

		# Extract the counts
		foreach var {pass fail skip tests} {
			incr total($var) [$i eval "set testinfo(num$var)"]
		}

		$i delete
		stdout flush
	}
	puts [string repeat = 73]
	puts [format "%16s: Total %5d   Passed %5d  Skipped %5d  Failed %5d" \
			Totals $total(tests) $total(pass) $total(skip) $total(fail)]

	if {$total(fail)} {
		exit 1
	}
}
