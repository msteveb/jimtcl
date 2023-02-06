#!/usr/bin/env jimsh

# Testing redis client access in non-blocking mode

# Requires the redis extension
package require redis

# A redis server should be running either on localhost 6379
# or on the given address (e.g. host:port)
try {
	lassign $argv addr
	if {$addr eq ""} {
		set addr localhost:6379
	}
	set s [socket stream $addr]
	# socket must be in non-blocking mode
	$s ndelay 1
	set r [redis -async $s]
} on error msg {
	puts [errorInfo $msg]
	exit 1
}

# List of outstanding redis commands
set cmds {}

$r readable {
	while {1} {
		set result [$r -type read]
		if {$result eq ""} {
			break
		}
		set cmds [lassign $cmds cmd]
		# Show command and response
		puts "$cmd => $result"
	}
}

# queue a command and remember it
proc redis_command {r args} {
	global cmds
	lappend cmds $args
	$r {*}$args
}

redis_command $r SET zz 0

proc periodic {r} {
	global counter done

	if {[incr counter] > 10} {
		incr done
	} else {
		redis_command $r INCR zz
		after 100 periodic $r
	}
}

set counter 0
periodic $r

vwait done
