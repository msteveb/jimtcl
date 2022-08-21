#!/usr/bin/env jimsh

# Shows what [$handle filename] returns for each socket type

foreach {type addr} {
	dgram {}
	dgram.server 5000
	dgram 127.0.0.1:5000
	pair {}
	pipe {}
	pty {}
	stream.server 127.0.0.1:5002
	stream 127.0.0.1:5002
	unix.server /tmp/uds.socket
	unix /tmp/uds.socket
	unix.dgram.server /tmp/uds.dgram.socket
	unix.dgram /tmp/uds.dgram.socket
} {
	try {
		set socks [socket $type {*}$addr]
		foreach s $socks {
			puts "$type $addr => [$s filename]"
		}
	} on error msg {
		puts "$type $addr => $msg"
	}
}
