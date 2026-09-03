#!/usr/bin/env jimsh

# Requires the redis extension
package require redis

# A redis server should be running either on localhost 6379
# or on the given host port
#
# Usage: redis-pubsub.tcl ?pub|sub? ?host:addr?
#
# If pub or sub is not given, forks and does both

if {[lindex $argv 0] in {pub sub}} {
    # Run in single process mode
    set argv [lassign $argv op]
} else {
    # fork before connecting so that both processes don't share
    # a connection
    if {[os.fork] == 0} {
        # child subscribes
        set op sub
    } else {
        set op pub
    }
}

try {
    lassign $argv addr
    if {$addr eq ""} {
            set addr localhost:6379
    }
    set r [redis [socket stream $addr]]
} on error msg {
    puts [errorInfo $msg]
    exit 1
}

if {$op eq "sub"} {
    # read will generate a bgerror if the server goes away
    proc bgerror {msg} {
        set ::done "$::op: $msg"
    }

    $r SUBSCRIBE chin
    $r SUBSCRIBE chan

    proc sub_timeout {} {
        set ::done "$::op: quitting on idle"
    }

    $r readable {
        after cancel $afterid
        set result [$r read]
        puts "$op: $result"
        set afterid [after 2000 sub_timeout]
    }
    # If no message for 2 seconds, stop
    set afterid [after 2000 sub_timeout]
    vwait done
    puts $done
} else {
    try {
        loop i 1 15 {
            $r PUBLISH chan PONG$i
            puts "$op: chan PONG$i"
            after 250
            $r PUBLISH chin PING$i
            puts "$op: chin PING$i"
            after 250
        }
    } on error msg {
        puts "$op: $msg"
    }
}
