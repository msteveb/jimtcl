proc copy_file {infile outfile} {
	set in [open $infile r]
	set out [open $outfile w]
	while {1} {
		set buf [read $in 200]
		if {[string length $buf] == 0} {
			break
		}
		puts -nonewline $out $buf
	}
	close $in
	close $out
}

proc check_file {message filename} {
	# Does it look OK?
	set line 0
	if {[catch {exec cmp -s test.bin $filename} err]} {
		puts "$message"
		puts "=========================================="
		puts "$filename did not match test.bin"
		error failed
	}
	verbose "$message -- ok"
}

check_file "Initial test.bin" test.bin

copy_file test.bin stdio.copy
check_file "Copy file with stdio" stdio.copy
file delete stdio.copy
