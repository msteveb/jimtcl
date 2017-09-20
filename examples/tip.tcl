#!/usr/bin/env jimsh

# tip.tcl is like a simple version of cu, written in pure Jim Tcl
# It makes use of the new aio tty support

# Note: On Mac OS X, be sure to open /dev/cu.* devices, not /dev/tty.* devices

set USAGE \
{Usage: tip ?settings? device
    or tip help

Where settings are as follows:
1|2             stop bits   (default 1)
5|6|7|8         data bits   (default 8)
even|odd        parity      (default none)
xonxoff|rtscts  handshaking (default none)
<number>        baud rate   (default 115200)

e.g. tip 9600 8 1 rtscts /dev/ttyUSB0}

set settings {
	baud 115200
	stop 1
	data 8
	parity none
	handshake none
	input raw
	output raw
	vmin 1
	vtime 1
}

set showhelp 0

foreach i $argv {
	if {[string match -h* $i] || [string match help* $i]} {
		puts $USAGE
		return 0
	}
	if {$i in {even odd}} {
		set settings(parity) $i
		continue
	}
	if {$i in {ixonixoff rtscts}} {
		set settings(handshake) $i
		continue
	}
	if {$i in {1 2}} {
		set settings(stop) $i
		continue
	}
	if {$i in {5 6 7 8}} {
		set settings(data) $i
		continue
	}
	if {[string is integer -strict $i]} {
		set settings(baud) $i
		continue
	}
	if {[file exists $i]} {
		set device $i
		continue
	}
	puts "Warning: unrecognised setting $i"
}

if {![exists device]} {
	puts $USAGE
	exit 1
}

# save stdin and stdout tty settings
# note that stdin and stdout are probably the same file descriptor,
# but it doesn't hurt to treat them independently
set stdin_save [stdin tty]
set stdout_save [stdout tty]

try {
	set f [open $device r+]
} on error msg {
	puts "Failed to open $device"
	return 1
}

if {[$f lock] == 0} {
	puts "Device is in use: $device"
	return 1
}

try {
	$f tty {*}$settings
} on error msg {
	puts "$device: $msg"
	return 1
}

puts "\[$device\] Use ~. to exit"

$f ndelay 1
$f buffering none

stdin tty input raw
stdin ndelay 1

stdout tty output raw
stdout buffering none

set status ""
set tilde 0
set tosend {}

# To avoid sending too much data and blocking,
# this sends str in chunks of 1000 bytes via writable
proc output-on-writable {fh str} {
	# Add it to the buffer to send
	append ::tosend($fh) $str

	if {[string length [$fh writable]] == 0} {
		# Start the writable event handler
		$fh writable [list output-is-writable $fh]
	}
}

# This is the writable callback
proc output-is-writable {fh} {
	global tosend
	set buf $tosend($fh)
	if {[string bytelength $buf] >= 1000} {
		set tosend($fh) [string byterange $buf 1000 end]
		set buf [string byterange $buf 0 999]
	} else {
		set tosend($fh) {}
		# All sent, so cancel the writable event handler
		$fh writable {}
	}
	$fh puts -nonewline $buf
}

proc bgerror {args} {
	set status $args
	incr ::done
}

# I/O loop

$f readable {
	set c [$f read]
	if {[$f eof]} {
		set status "$device: disconnected"
		incr done
		break
	}
	output-on-writable stdout $c
}

proc tilde_timeout {} {
	global tilde f
	if {$tilde} {
		output-on-writable $f ~
		set tilde 0
	}
}

stdin readable {
	set c [stdin read]
	# may receive more than one char here, but only need to consider
	# ~. processing if we receive them as separate chars
	if {$tilde == 0 && $c eq "~"} {
		incr tilde
		# Need ~. within 1 second of each other
		after 1000 tilde_timeout
	} else {
		if {$tilde} {
			after cancel tilde_timeout
			set tilde 0
			if {$c eq "."} {
				incr done
				return
			}
			output-on-writable $f ~
		}
		output-on-writable $f $c
	}
}

vwait done

# restore previous settings
stdin tty {*}$stdin_save
stdout tty {*}$stdout_save

puts $status
