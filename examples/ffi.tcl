package require ffi

proc sizeof_example {} {
	puts [[ffi.int] size]
	puts [[ffi.long] size]
	puts [[ffi.pointer] size]
}

proc types_example {} {
	# variables are created using ffi.int, ffi_uint, ffi.long and so on; they
	# can be initialized by specifying a value
	set count [ffi.int 12345678]
	set uninit_count [ffi.int]

	# "value" returns the value of a variable as a Tcl object and "address"
	# returns its address (like the & operator in C)
	puts "integer with contents [$count value] is at [$count address]"
	puts "the value of the uninitialized integer is [$uninit_count value]"

	# the raw contents of a variable may be accessed using "raw"
	puts "the raw, [$count size] bytes form of [$count value] is [$count raw]"

	# pointers are created by calling ffi.pointer with the address of a variable
	set count_ptr [ffi.pointer [$count address]]
	puts [$count_ptr value]

	# strings are created using "ffi.string copy" or "ffi.string at"; the latter
	# is also used to read strings (i.e the return value of a C function that
	# returns a char *) and dereference pointers
	set s [ffi.string copy "this is a string"]
	puts [$s value]
}

proc functions_example {} {
	# functions are accessed through the return value of "ffi.dlopen" - the
	# first argument is the return value type, the second is the function symbol
	# name and the rest are argument types
	set libc [ffi.dlopen libc.so.6]
	set puts_ptr [$libc int puts pointer]

	# functions are called by using the function object as the command; the
	# return value object and the parameters must be passed using their
	# addresses
	set ret [ffi.int]
	set msg [ffi.string copy "print this please"]
	$puts_ptr [$ret address] [$msg address]
	puts "the return value of puts() is [$ret value]"

	# (it's also possible to create temporary variables in case you don't want
	# to read the variable values)
	$puts_ptr [[ffi.int] address] [$msg address]

	# let's do this again - this time, with a function with a more complex
	# prototype: sprintf()
	set sprintf_ptr [$libc int sprintf pointer pointer pointer int]

	# "ffi.buffer" is a quick, efficient way to allocate buffers with a given
	# size
	set buf [ffi.buffer 32]
	$sprintf_ptr [[ffi.int] address] [$buf address] [[ffi.string copy "%s %d"] address] [[ffi.string copy "aha"] address] [[ffi.int 1337] address]

	# print the output buffer (the first argument)
	puts [$buf value]
}

proc constants_example {} {
	# ::main is the the main executable handle (see dlopen(3)) - on some
	# platforms, the loader resolves symbols recursively, so there's no need to
	# load obtain a libc handle
	set exit_ptr [$::main void exit int]

	# ::null (a global) is a NULL pointer
	puts [$::null value]

	# ::zero is 0 (int), useful for functions that accept flags
	puts [$::zero value]

	# ::one is 1 (int), useful for functions that accept an int that acts as a
	# boolean (i.e setsockopt())
	puts [$::one value]

	# make sure:
	# 1) you don't modify these globals by mistake: don't pass them to functions
	#    unless they don't modify them (i.e they're const parameters)
	# 2) you use them whenever possible, instead of redefining them: this
	#    improves efficiency (think of it - why create a new NULL pointer object
	#    if you already have one?) and improves code clarity
}

proc pointers_example {} {
	# call time() to get the current time
	set time_ptr [$::main void time pointer]
	set now [ffi.int]
	$time_ptr [[ffi.void] address] [[ffi.pointer [$now address]] address]

	# call gmtime(), which returns a struct tm pointer
	set gmtime_ptr [$::main pointer gmtime pointer]
	set now_ptr [ffi.pointer [$now address]]
	set now_broken [ffi.pointer]
	$gmtime_ptr [$now_broken address] [$now_ptr address]

	# call asctime() and pass the struct tm pointer
	set asctime_ptr [$::main pointer asctime pointer]
	set now_broken_ptr [ffi.pointer [$now_broken value]]
	set out [ffi.pointer]
	$asctime_ptr [$out address] [$now_broken_ptr address]

	# asctime() returns a string - print it
	puts [ffi.string at [$out value] 24]
}

proc structs_example {} {
	# locate asctime()
	set asctime_ptr [$::main pointer asctime pointer]

	# create a struct tm and initialize it with January 30th 1992, 2:10 AM;
	# structs are initialized using their raw value
	set now_broken [ffi.struct "[$::zero raw][[ffi.int 10] raw][[ffi.int 2] raw][[ffi.int 30] raw][$::zero raw][[ffi.int 92] raw][$::zero raw][[ffi.int 30] raw][$::zero raw][[ffi.long 0] raw][$::null raw]" int int int int int int int int int long pointer]
	set struct_tm_size [$now_broken size]

	# for demonstration purposes, read tm_year (the 6th member of struct tm),
	# using "member" and the zero-based member index
	set tm_year 5
	puts [[$now_broken member $tm_year] value]

	# call asctime()
	set now_broken_ptr [ffi.pointer [$now_broken address]]
	set out [ffi.pointer]
	$asctime_ptr [$out address] [$now_broken_ptr address]
	puts [ffi.string at [$out value] 24]

	# now, let's do this again: this time, with an uninitialized struct
	set now_broken [ffi.struct "" int int int int int int int int int long pointer]
	set now_broken_ptr [ffi.pointer [$now_broken address]]
	$asctime_ptr [$out address] [$now_broken_ptr address]
	puts [ffi.string at [$out value] 24]

	# ... and again: this time with a zeroed struct tm
	set now_broken [ffi.struct "[string repeat \x00 [expr $struct_tm_size - 1]]]" int int int int int int int int int long pointer]
	set now_broken_ptr [ffi.pointer [$now_broken address]]
	$asctime_ptr [$out address] [$now_broken_ptr address]
	puts [ffi.string at [$out value] 24]
}

proc sockets_example {} {
	# this is a fairly complex, non-trivial example; it may not work on
	# architectures other than x86, because of the size and alignment of struct
	# sockaddr_in, socklen_t, etc'

	set AF_INET 2
	set SOCK_STREAM 1
	set INADDR_LOOPBACK 0x7F000001

	set socket_ptr [$::main int socket int int int]
	set htons_ptr [$::main uint16 htons uint16]
	set htonl_ptr [$::main uint32 htonl uint32]
	set bind_ptr [$::main int bind int pointer uint32]
	set listen_ptr [$::main int listen int int]
	set accept_ptr [$::main int accept int pointer pointer]
	set send_ptr [$::main int send int pointer uint int]
	set close_ptr [$::main int close int]

	set ret [ffi.int]

	# format a struct sockaddr_in structure (127.0.0.1:9000)
	set port [ffi.uint16]
	$htons_ptr [$port address] [[ffi.uint16 9000] address]
	set addr [ffi.uint32]
	$htonl_ptr [$addr address] [[ffi.uint32 $INADDR_LOOPBACK] address]
	set listen_addr [ffi.struct "[[ffi.ushort $AF_INET] raw][$port raw][$addr raw][[ffi.uint64] raw]" ushort uint16 uint32 uint64]

	# create a socket
	set s [ffi.int]
	$socket_ptr [$s address] [[ffi.int $AF_INET] address] [[ffi.int $SOCK_STREAM] address] [$::zero address]
	if {[$s value] < 0} {
		return
	}

	try {
		# start listening
		$bind_ptr [$ret address] [$s address] [[ffi.pointer [$listen_addr address]] address] [[ffi.int [$listen_addr size]] address]
		if {[$ret value] < 0} {
			throw error "failed to bind the socket"
		}

		$listen_ptr [$ret address] [$s address] [[ffi.int 5] address]
		if {[$ret value] < 0} {
			throw error "failed to listen"
		}

		# accept a client
		set client_addr [ffi.struct "[[ffi.ushort] raw][[ffi.uint16] raw][[ffi.uint32] raw][[ffi.uint64] raw]" ushort uint16 uint32 uint64]
		set c [ffi.int]
		puts "waiting for a client on port 9000"
		$accept_ptr [$c address] [$s address] [[ffi.pointer [$client_addr address]] address] [[ffi.pointer [[ffi.int [$client_addr size]] address]] address]
		if {[$c value] < 0} {
			throw error "failed to accept a client"
		}

		try {
			# send something
			$send_ptr [[ffi.int] address] [$c address] [[ffi.string copy hello] address] [[ffi.uint 5] address] [$::zero address]
		} finally {
			$close_ptr [[ffi.int] address] [$c address]
		}
	} on error {msg opts} {
		puts "Error: $msg"
	} finally {
		# pay attention: correct error handling is crucial when working with
		# file descriptors directly
		$close_ptr [[ffi.int] address] [$s address]
	}
}

sizeof_example
types_example
functions_example
constants_example
pointers_example
structs_example
sockets_example
