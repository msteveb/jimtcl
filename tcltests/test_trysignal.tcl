signal handle ALRM

alarm 1
try -signal {
	foreach i {1 2 3 4 5} {
		sleep 0.4
	}
	set msg ""
} on signal {msg} {
	# Just set msg here
} finally {
	alarm 0
}

check trysignal.1 $msg SIGALRM
check trysignal.2 [expr {$i in {2 3}}] 1
