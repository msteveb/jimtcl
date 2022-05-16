#!/usr/bin/env jimsh

# A simple test of the redis extension

# Requires the redis extension
package require redis

# A redis server should be running either on localhost 6379
# or on the given address (e.g. host:port)
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

puts "KEYS: [$r KEYS *]"

# Set a hash
set env(testing) yes
$r HMSET env {*}$env

set result [$r HGET env testing]
puts "HGET: testing=$result"

# Now the same with -type
set result [$r -type HGET env testing]
puts "HGET (-type): testing=$result"

# Now a missing value with -type
set result [$r -type HGET env doesnotexist]
puts "HGET (-type): doesnotexist=$result"

set result [$r -type HGETALL env]
puts "HGETALL (-type): $result"

set size [$r HLEN env]
puts "Size of env is $size"

set time [time {
	$r HGETALL env
} 100]
puts "HGETALL: $time"

# a multi-command transation
$r MULTI
$r SET a A1
$r SET b B2
$r EXEC
puts "MGET: [$r MGET a b]"

# disard
$r MULTI
$r SET a ~A1
$r SET b ~B2
$r DISCARD
puts "MGET (DISCARD): [$r MGET a b]"

set result [$r HGET env testing]

$r close
