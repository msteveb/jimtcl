proc bgerror {msg} {
	#puts "bgerror: $msg"
	#exit 0
}

if {[os.fork] == 0} {
	puts "child: waiting a bit"

	# This will be our client

	sleep .1

	set f [aio.socket stream localhost:9876]

	set done 0

	proc onread {f} {
		if {[$f gets buf] > 0} {
			puts "child: read: $buf"
		} else {
			puts "child: read got eof"
			close $f
			set ::done 1
			$f readable {}
		}
	}

	proc oneof {f} {
		$f close
		puts "child: eof so closing"
		set ::done 1
	}

	proc onwrite {f} {
		puts "child: sending request"
		$f puts -nonewline "GET / HTTP/1.0\r\n\r\n"
		$f flush
		$f writable {}
	}

	$f readable {onread $f} {oneof $f}
	$f writable {onwrite $f}

	alarm 10
	catch -signal {
		puts "child: in event loop"
		vwait done
		puts "child: done event loop"
	}
	alarm 0
	exit 0
}

puts "parent: opening socket"
set done 0

# This will be our server
set f [aio.socket stream.server 0.0.0.0:9876]

proc server_onread {f} {
	puts "parent: onread (server) got connection on $f"
	set cfd [$f accept]
	puts "parent: onread accepted $cfd"

	$cfd puts "Thanks for the request"
	$cfd close

	puts "parent: sent response"

	incr ::done
}

$f readable {server_onread $f}

alarm 10
catch -signal {
	vwait done
}
alarm 0
