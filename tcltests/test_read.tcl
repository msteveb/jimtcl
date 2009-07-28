set f [open "/dev/urandom" r]

set count 0
set error NONE

signal handle SIGALRM
catch -signal {
	alarm 0.5
	while {1} {
		incr count [bio read -hex $f buf 1]
	}
	alarm 0
	signal default SIGALRM
} error

verbose "Read $count bytes in 0.5 seconds: Got $error"

# Kill it off
#kill -TERM [pid $f]
catch {close $f}
