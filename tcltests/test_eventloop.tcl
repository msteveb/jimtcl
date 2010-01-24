if {[info commands vwait] eq ""} {
	return "noimpl"
}

set f [socket stream localhost:80]

set count 0
set done 0

proc onread {f} {
	#puts "[$f gets]"
	incr ::count [string length [$f gets]]
}

proc oneof {f} {
	$f close
	verbose "Read $::count bytes from server"
	incr ::done
}

proc onwrite {f} {
	$f puts -nonewline "GET / HTTP/1.0\r\n\r\n"
	$f flush
	$f writable {}
}

proc bgerror {msg} {
	puts stderr "bgerror: $msg"
	incr ::done
}

$f readable {onread $f} {oneof $f}
$f writable {onwrite $f}

alarm 10
catch -signal {
	vwait done
}
alarm 0
catch {close $f}

rename bgerror ""
rename onread ""
rename oneof ""
rename onwrite ""

return
