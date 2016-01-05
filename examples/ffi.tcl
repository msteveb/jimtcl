package require ffi

proc sizeof_example {} {
	# standard types - those that begin with "u" are unsigned
	puts [[ffi::ulong] size]
	puts [[ffi::long] size]
	puts [[ffi::uint] size]
	puts [[ffi::int] size]
	puts [[ffi::ushort] size]
	puts [[ffi::short] size]
	puts [[ffi::uchar] size]
	puts [[ffi::char] size]

	# fixed-width types ({u,}int64 may be missing on pure 32-bit architectures)
	# puts [[ffi::uint64] size]
	# puts [[ffi::int64] size]
	puts [[ffi::uint32] size]
	puts [[ffi::int32] size]
	puts [[ffi::uint16] size]
	puts [[ffi::int16] size]
	puts [[ffi::uint8] size]
	puts [[ffi::int8] size]

	# floating-point types
	puts [[ffi::float] size]
	puts [[ffi::double] size]

	# pointers; see ffi::buffer and ffi::string later
	puts [[ffi::pointer] size]

	# misc. types
	puts [[ffi::void] size]
}

proc types_example {} {
	# variables are created using ffi::int, ffi::uint, ffi::long and so on; if
	# no value is specified, variables are initialized with either 0, \0 or NULL
	set count [ffi::int 12345678]
	set zero_count [ffi::int]

	# the value method returns the value of a variable as a Tcl object and
	# the method address returns its address (like the & operator in C) in
	# hexadecimal form
	puts "integer with contents [$count value] is at [$count address]"
	puts "the value of the integer created with no value is [$zero_count value]"

	# the raw contents of a variable may be accessed using the raw method
	puts "the raw, [$count size] bytes form of [$count value] is [$count raw]"

	# pointers are created by calling ffi::pointer with the address of a
	# variable; that's the only pointer type (i.e there's no int * equivalent,
	# see ffi:string at and ffi::cast)
	set count_func [ffi::pointer [$count address]]
	puts "the pointer points to [$count_func value]"

	# strings are created using ffi::string copy or ffi::string at; the latter
	# is also used to read strings (i.e the return value of a C function that
	# returns a char *) and dereference pointers
	set s [ffi::string copy "this is a string"]
	puts [$s value]
}

proc functions_example {} {
	# shared libraries are loaded using ffi::dlopen
	set libc [ffi::dlopen libc.so.6]

	# the handle method returns the handle (i.e return value of dlopen()) of a
	# shared library
	puts "the handle of libc.so.6 is [$libc handle]"

	# functions are accessed through the dlsym method of a library and
	# ffi::function; the first argument is the return value type, the second is
	# the function address and the rest are argument types
	set puts_addr [$libc dlsym puts]
	puts "puts() is at $puts_addr"
	set puts_func [ffi::function int $puts_addr pointer]

	# functions are called by using the function object as the command; the
	# return value object and the parameters must be passed using their
	# addresses
	set ret [ffi::int]
	set msg [ffi::string copy "print this please"]
	$puts_func [$ret address] [$msg address]
	puts "the return value of puts() is [$ret value]"

	# (it's also possible to create temporary variables in case you don't want
	# to read the variable values)
	$puts_func [[ffi::int] address] [$msg address]

	# let's do this again - this time, with a function with a more complex
	# prototype: sprintf()
	set sprintf_func [ffi::function int [$libc dlsym sprintf] pointer pointer pointer int]

	# ffi::buffer is a quick, efficient way to allocate buffers with a given
	# size
	set buf [ffi::buffer 32]
	$sprintf_func [[ffi::int] address] [$buf address] [[ffi::string copy "%s %d"] address] [[ffi::string copy "aha"] address] [[ffi::int 1337] address]

	# print the output buffer (the first argument)
	puts [$buf value]
}

proc constants_example {} {
	# ::main is the the main executable handle (see dlopen(3)) - on some
	# platforms, the loader resolves symbols recursively, so there's no need to
	# load obtain a libc handle
	puts "exit() is at [$::main dlsym exit]"

	# ::null (a global) is a NULL pointer
	puts "the value of ::null is [$::null value]"

	# ::zero is 0 (int), useful for functions that accept flags
	puts "the value of ::zero is [$::zero value]"

	# ::one is 1 (int), useful for functions that accept an int that acts as a
	# boolean (i.e setsockopt())
	puts "the value of ::one is [$::one value]"

	# make sure:
	# 1) you don't modify these globals by mistake: don't pass them to functions
	#    unless they don't modify them (i.e they're const parameters)
	# 2) you use them whenever possible, instead of redefining them: this
	#    improves efficiency (think of it - why create a new NULL pointer object
	#    if you already have one?) and improves code clarity
}

proc pointers_example {} {
	# call time() to get the current time
	set time_func [ffi::function void [$::main dlsym time] pointer]
	set now [ffi::int]
	$time_func [[ffi::void] address] [[ffi::pointer [$now address]] address]

	# call gmtime(), which returns a struct tm pointer
	set gmtime_func [ffi::function pointer [$::main dlsym gmtime] pointer]
	set now_func [ffi::pointer [$now address]]
	set now_broken [ffi::pointer]
	$gmtime_func [$now_broken address] [$now_func address]

	# call asctime() and pass the struct tm pointer
	set asctime_func [ffi::function pointer [$::main dlsym asctime] pointer]
	set now_broken_func [ffi::pointer [$now_broken value]]
	set out [ffi::pointer]
	$asctime_func [$out address] [$now_broken_func address]

	# asctime() returns a string - print it
	puts [ffi::string at [$out value] 24]
}

proc structs_example {} {
	# locate asctime()
	set asctime_func [ffi::function pointer [$::main dlsym asctime] pointer]

	# create a struct tm and initialize it with January 30th 1992, 2:10 AM;
	# structs are initialized using their raw value
	set now_broken [ffi::struct "[$::zero raw][[ffi::int 10] raw][[ffi::int 2] raw][[ffi::int 30] raw][$::zero raw][[ffi::int 92] raw][$::zero raw][[ffi::int 30] raw][$::zero raw][[ffi::long 0] raw][$::null raw]" int int int int int int int int int long pointer]

	# it is also possible to pass an empty string instead of an initializer - in
	# this case, the struct is filled with zero bytes
	set now_zero [ffi::struct "" int int int int int int int int int long pointer]

	# the size method returns the size of a struct (like sizeof())
	set struct_tm_size [$now_broken size]
	puts "the size of struct tm is $struct_tm_size bytes"

	# for demonstration purposes, read tm_year (the 6th member of struct tm),
	# using "member" and the zero-based member index
	set tm_year 5
	puts "tm_year of the struct_tm at [$now_broken address] is [[$now_broken member $tm_year] value]"

	# call asctime()
	set now_broken_func [ffi::pointer [$now_broken address]]
	set out [ffi::pointer]
	$asctime_func [$out address] [$now_broken_func address]
	puts [ffi::string at [$out value] 24]

	# now, let's do this again: this time, with an uninitialized struct
	set now_broken [ffi::struct "" int int int int int int int int int long pointer]
	set now_broken_func [ffi::pointer [$now_broken address]]
	$asctime_func [$out address] [$now_broken_func address]
	puts [ffi::string at [$out value] 24]

	# ... and again: this time with a zeroed struct tm and without passing the
	# string length to ffi::string at (so it guesses it using strlen())
	set now_broken [ffi::struct "[string repeat \x00 [expr $struct_tm_size - 1]]]" int int int int int int int int int long pointer]
	set now_broken_func [ffi::pointer [$now_broken address]]
	$asctime_func [$out address] [$now_broken_func address]
	puts [ffi::string at [$out value]]
}

proc arrays_example {} {
	# arrays are created almost like structs, using an initializer (or an empty
	# string), the type of all elements and the array length
	set numbers [ffi::array [[ffi::long 0x10] raw][[ffi::long 0x20] raw] long 2]

	puts "the array length is [$numbers length]"
	puts "the array size is [$numbers size] bytes"
	puts "the first element in the array is [[$numbers member 0] value]"
}

proc cast_example {} {
	# ffi::cast can be used to create an object with with the value at a given
	# address: it's equivalent to the * operator in C, after casting from void *
	# to another pointer type (e.g int *)
	set errno_addr [$::main dlsym errno]
	set errno [ffi::cast int $errno_addr]
	puts "the value of errno before close() is [$errno value]"

	# now, we call close() with an invalid file descriptor so it puts EBADF in
	# errno
	set close_func [ffi::function int [$::main dlsym close] int]
	set ret [ffi::int]
	$close_func [$ret address] [[ffi::int 0xFF] address]
	puts "close() returned [$ret value]"

	# we have to re-read errno!
	set errno [ffi::cast int $errno_addr]

	# call strerror() and pass the value of errno
	set strerror_func [ffi::function pointer [$::main dlsym strerror] int]
	set str [ffi::pointer]
	$strerror_func [$str address] [[ffi::int [$errno value]] address]

	puts "the error in errno after close() is: [ffi::string at [$str value]] ([$errno value])"
}

proc safety_example {} {
	# ffi::string, ffi::function and ffi::cast throw an error upon attempt to
	# dereference a NULL pointer, but all other memory and pointer operations
	# are unsafe and may lead to crashes, as in C
	try {
		ffi::string at [$::null value]
	} on error {msg opts} {
		puts "Caught an exception: $msg"
	}

	try {
		ffi::function void [$::null value]
	} on error {msg opts} {
		puts "Caught an exception: $msg"
	}

	try {
		ffi::cast int [$::null value]
	} on error {msg opts} {
		puts "Caught an exception: $msg"
	}
}

proc sockets_example {} {
	# this is a fairly complex, non-trivial example; it may not work on
	# architectures other than x86, because of the size and alignment of struct
	# sockaddr_in, socklen_t, etc'

	set AF_INET 2
	set SOCK_STREAM 1
	set INADDR_LOOPBACK 0x7F000001

	set socket_func [ffi::function int [$::main dlsym socket] int int int]
	set htons_func [ffi::function uint16 [$::main dlsym htons] uint16]
	set htonl_func [ffi::function uint32 [$::main dlsym htonl] uint32]
	set bind_func [ffi::function int [$::main dlsym bind] int pointer uint32]
	set listen_func [ffi::function int [$::main dlsym listen] int int]
	set accept_func [ffi::function int [$::main dlsym accept] int pointer pointer]
	set send_func [ffi::function int [$::main dlsym send] int pointer uint int]
	set close_func [ffi::function int [$::main dlsym close] int]

	set ret [ffi::int]

	# format a struct sockaddr_in structure (127.0.0.1:9000)
	set port [ffi::uint16]
	$htons_func [$port address] [[ffi::uint16 9000] address]
	set addr [ffi::uint32]
	$htonl_func [$addr address] [[ffi::uint32 $INADDR_LOOPBACK] address]
	set listen_addr [ffi::struct "[[ffi::ushort $AF_INET] raw][$port raw][$addr raw][[ffi::uint64] raw]" ushort uint16 uint32 uint64]

	# create a socket
	set s [ffi::int]
	$socket_func [$s address] [[ffi::int $AF_INET] address] [[ffi::int $SOCK_STREAM] address] [$::zero address]
	if {[$s value] < 0} {
		return
	}

	try {
		# start listening
		$bind_func [$ret address] [$s address] [[ffi::pointer [$listen_addr address]] address] [[ffi::int [$listen_addr size]] address]
		if {[$ret value] < 0} {
			throw error "failed to bind the socket"
		}

		$listen_func [$ret address] [$s address] [[ffi::int 5] address]
		if {[$ret value] < 0} {
			throw error "failed to listen"
		}

		# accept a client
		set client_addr [ffi::struct "[[ffi::ushort] raw][[ffi::uint16] raw][[ffi::uint32] raw][[ffi::uint64] raw]" ushort uint16 uint32 uint64]
		set c [ffi::int]
		puts "waiting for a client on port 9000"
		$accept_func [$c address] [$s address] [[ffi::pointer [$client_addr address]] address] [[ffi::pointer [[ffi::int [$client_addr size]] address]] address]
		if {[$c value] < 0} {
			throw error "failed to accept a client"
		}

		# send something
		$send_func [[ffi::int] address] [$c address] [[ffi::string copy hello\n] address] [[ffi::uint 6] address] [$::zero address]

		# disconnect the client
		$close_func [[ffi::int] address] [$c address]
	} on error {msg opts} {
		puts "Error: $msg"
	} finally {
		# pay attention: correct error handling is crucial when working with
		# file descriptors directly
		$close_func [[ffi::int] address] [$s address]
	}
}

sizeof_example
types_example
functions_example
constants_example
pointers_example
structs_example
arrays_example
cast_example
safety_example
sockets_example
