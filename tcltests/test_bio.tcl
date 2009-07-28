proc copy_binary_file {infile outfile} {
	set in [open $infile r]
	set out [open $outfile w]
	while {[bio read $in buf 200] > 0} {
		bio write $out $buf
	}
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
	set expected {
		000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d
		1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b
		3c3d3e3f404142434445464748494a4b4c4d4e4f50515253545556575859
		5a5b5c5d5e5f606162636465666768696a6b6c6d6e6f7071727374757677
		78797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495
		969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3
		b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1
		d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeef
		f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff000102
	}

	# Does it look OK?
	set line 0
	exec xxd -p $filename >tmp.1
	set f [open "tmp.1" r]
	while {[gets $f buf] >= 0} {
		if {[lindex $expected $line] != $buf} {
			incr line
			puts $message
			puts "=========================================="
			puts "Failed match on line $line"
			puts "Exp: {[lindex $expected $line]}"
			puts "Got: {$buf}"
			error failed
		}
		incr line
	}
	close $f
	verbose "$message -- ok"
}

# First create a binary file with the chars 0 - 255
set f [open bio.test w]
bio write $f "\0010"
bio write $f "\0011"
for {set i 2} {$i < 256} {incr i} {
	puts -nonewline $f [format %c $i]
}
bio write $f "\0010\0011\002"
close $f
check_file "Create binary file from std encoding" bio.test

# Now the same using hex mode
set hex ""
for {set i 0} {$i < 256} {incr i} {
	append hex [format %02x $i]
}
append hex 000102
set f [open bio.test w]
bio write -hex $f $hex
close $f
check_file "Create binary file from hex encoding" bio.test

copy_binary_file bio.test bio.copy
check_file "Copy binary file with std encoding" bio.copy
copy_binary_file_hex bio.test bio.copy
check_file "Copy binary file with hex encoding" bio.copy
copy_file bio.test bio.copy
check_file "Copy file with stdio" bio.copy
file delete bio.test
file delete bio.copy
