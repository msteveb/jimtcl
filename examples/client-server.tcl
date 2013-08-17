proc bgerror {msg} {
	puts "bgerror: $msg"
	#exit 0
}

proc verbose {msg} {
	puts $msg
}

if {[os.fork] == 0} {
	verbose "child: waiting a bit"

	# This will be our client

	sleep .1

	set f [socket stream localhost:9876]
	fconfigure $f -buffering line

	set done 0

	proc onread {f} {
		if {[$f gets buf] > 0} {
			verbose "child: read response '$buf'"
		} else {
			verbose "child: read got eof"
			set ::done 1
		}
	}

	proc onwrite {f} {
		verbose "child: sending request"
		$f puts -nonewline "GET / HTTP/1.0\r\n\r\n"
		$f writable {}
	}

	$f writable [list onwrite $f]
	$f readable [list onread $f]

	alarm 10
	catch -signal {
		verbose "child: in event loop"
		vwait done
		verbose "child: done event loop"
	}
	alarm 0
	$f close
	exit 0
}

verbose "parent: opening socket"
set done 0

# This will be our server
set f [socket stream.server 0.0.0.0:9876]

proc server_onread {f} {
	verbose "parent: onread (server) got connection on $f"
	set cfd [$f accept]
	verbose "parent: onread accepted $cfd"

	verbose "parent: read request '[string trim [$cfd gets]]'"

	$cfd puts "Thanks for the request"
	$cfd close

	verbose "parent: sent response"

	incr ::done
}

$f readable [list server_onread $f]

alarm 10
catch -signal {
	vwait done
}
alarm 0
$f close

sleep .5

return "ok"
