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
	$sprintf_ptr [[ffi.int] address] "[$buf address] [[ffi.string copy "%s %d"] address] [[ffi.string copy "aha"] address] [[ffi.int 1337] address]"

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

sizeof_example
types_example
functions_example
constants_example
pointers_example
structs_example
