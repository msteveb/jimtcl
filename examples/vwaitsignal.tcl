# In lieu of some proper unit tests, this example
# checks that vwait can be interrupted by handled signals

set f [open "/dev/urandom" r]

set count 0

$f ndelay 1

signal handle SIGALRM

$f readable {
	incr count [string bytelength [read $f 100]]
}


set start [clock millis]

# Even though nothing sets 'done', vwait will return on the signal
alarm 0.5
vwait -signal done

set end [clock millis]
puts "Read $count bytes and received [signal check -clear] after $($end - $start)ms"

$f close
