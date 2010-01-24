if {[info commands bio] eq ""} {
	return "noimpl"
}
if {[info commands verbose] eq ""} {
	proc verbose {msg} {puts $msg}
}

proc copy_binary_file {infile outfile} {
	set in [open $infile r]
	set out [open $outfile w]
	while {[bio read $in buf 200] > 0} {
		bio write $out $buf
	}
	close $in
	close $out
}

proc copy_binary_file_direct {infile outfile} {
	set in [open $infile r]
	set out [open $outfile w]
	bio copy $in $out
	close $in
	close $out
}

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

proc copy_binary_file_hex {infile outfile} {
	set in [open $infile r]
	set out [open $outfile w]
	while {[bio read -hex $in buf 200] > 0} {
		bio write -hex $out $buf
	}
	close $in
	close $out
}

proc check_file {message filename} {
	# Does it look OK?
	set rc [catch {exec cmp -s $filename test.bin} error]
	if {$rc != 0} {
		puts "$message ($error)"
		puts "=========================================="
		puts "Did not match: $filename test.bin"
		error failed
	}
	verbose "$message -- ok"
}

# First create a binary file with the chars 0 - 255
set f [open bio.test w]
for {set i 0} {$i < 256} {incr i} {
	puts -nonewline $f [format %c $i]
}
close $f
check_file "Create binary file from std encoding" bio.test

# Now the same using hex mode
set hex ""
for {set i 0} {$i < 256} {incr i} {
	append hex [format %02x $i]
}
set f [open bio.test w]
bio write -hex $f $hex
close $f
check_file "Create binary file from hex encoding" bio.test

copy_binary_file bio.test bio.copy
check_file "Copy binary file with std encoding" bio.copy
copy_binary_file_direct bio.test bio.copy
check_file "Copy binary file with bio copy" bio.copy
copy_binary_file_hex bio.test bio.copy
check_file "Copy binary file with hex encoding" bio.copy
copy_file bio.test bio.copy
check_file "Copy file with stdio" bio.copy
file delete bio.test
file delete bio.copy
