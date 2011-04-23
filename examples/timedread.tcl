# Tests that SIGALRM can interrupt read
set f [open "/dev/urandom" r]

set count 0
set error NONE

signal handle SIGALRM
catch -signal {
	alarm 0.5
	while {1} {
		incr count [string bytelength [read $f 100]]
	}
	alarm 0
	signal default SIGALRM
} error

puts "Read $count bytes in 0.5 seconds: Got $error"

$f close
