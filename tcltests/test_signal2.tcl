signal ignore HUP TERM
signal handle ALRM INT

# Send both the handled signals.
# Should not exit here
alarm 1
kill -INT [pid]
sleep 2
set x 0
set signals {}
try -signal {
	# This should not execute
	incr x
} on signal {signals} {
}
check signal.1 $x 0
check signal.2 [lsort $signals] "SIGALRM SIGINT"

# Now no signals should be pending
set x 0
set signals {}
alarm 1
try -signal {
	kill -HUP [pid]
	signal throw TERM
	# Should get here
	incr x
	sleep 10
	# But not get here
	incr x
} on signal {signals} {
}

check signal.3 $x 1
check signal.4 [lsort $signals] "SIGALRM"
check signal.5 [lsort [signal check]] "SIGHUP SIGTERM"
check signal.6 [lsort [signal check SIGTERM]] "SIGTERM"
check signal.7 [lsort [signal check -clear SIGTERM]] "SIGTERM"
check signal.8 [lsort [signal check -clear]] "SIGHUP"
check signal.9 [lsort [signal check]] ""
