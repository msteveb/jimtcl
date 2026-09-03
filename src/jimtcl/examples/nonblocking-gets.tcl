#!/usr/bin/env jimsh

# Tests that 'gets' on a non-blocking socket
# does not return partial lines

lassign [socket pipe] r w

if {[os.fork] == 0} {
	# The child will be our client
	$r close
	# Output increasingly long lines
	loop i 10000 {
		$w puts [string repeat a $i]
	}
} else {
	# The server reads lines with gets.
	# Each one should be one longer than the last
	$w close

	set exp 0
	$r ndelay 1
	$r readable {
		while {[$r gets buf] >= 0} {
			set len [string length $buf]
			if {$len != $exp} {
				puts "Read line of length $len but expected $exp"
				incr done
				break
			}
			incr exp
		}
		if {[$r eof]} {
			incr done
		}
	}

	vwait done
}
