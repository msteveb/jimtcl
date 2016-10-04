###############################################################################
# Number of untested lines of code
#
# This number should only ever be reduced.
# i.e. new code must not add untested lines
# This should be changed with each commit which improves test coverage.

set untested_lines 2610


###############################################################################
# Number of code lines excluded from coverage stats
#
# This should ONLY be used where there is no way that a test for the code line(s)
# can be written.
# Code can be excluded as noted in https://linux.die.net/man/1/lcov

set excluded_lines 0

###############################################################################

puts "\n\n"
puts "Parsing test coverage results\n\n"

# Parse lcov summary with exclusion markers
set summary  [exec lcov --summary lcov.txt]
puts $summary

set pattern  {^\s+lines\.+\: (\d+\.\d+)\% \((\d+) of (\d+) lines\)$}
if { ! [regexp -line $pattern $summary whole percentage tested_lines total_lines] } {
	puts "Cannot find line summary"
	exit -1
}

# Parse lcov summary without exclusion markers
set summary  [exec lcov --summary lcov_output_without_exclusions.txt]
if { ! [regexp -line $pattern $summary whole percentage_nomarkers tested_lines_nomarkers total_lines_nomarkers] } {
	puts "Cannot find line summary"
	exit -1
}

set new_excluded_lines [expr { $total_lines_nomarkers - $total_lines }]
set new_untested_lines [expr { $total_lines - $tested_lines }]

set new_untested_or_excluded_lines  [expr $total_lines - $tested_lines + ($new_excluded_lines - $excluded_lines)]

set result 0

puts "Untested lines: $new_untested_lines (was $untested_lines)\n"
puts "Excluded lines: $new_excluded_lines (was $excluded_lines)\n"

if {$excluded_lines < $new_excluded_lines} {
	puts stderr "ERROR: Code added with test coverage exclusions. Add a justification to the commit message and update lcov_parse.pl with \$excluded_lines = $new_excluded_lines\n"
	set result -1
} elseif {$excluded_lines > $new_excluded_lines} {
	puts stderr "NOTE: Please update lcov_parse.pl with \$excluded_lines = $new_excluded_lines\n"
	set result -1
}

if {$untested_lines < $new_untested_lines} {
	puts stderr "ERROR: Untested code added. Add tests for the code before resubmitting commit.\n"
	set result  -1
} elseif {$untested_lines > $new_untested_lines} {
	if { ($new_excluded_lines - $excluded_lines) < ($new_untested_lines - $untested_lines) } {
		puts "Thank you for adding tests\n"
	}
	puts stderr "NOTE: Please update lcov_parse.pl with \$untested_lines = $new_untested_lines\n"
	set result -1
}

puts "\n"

exit $result
