package require ffi

# load the C libary
set libc [ffi.dlopen libc.so.6]

proc sizeof_example {} {
	puts [[ffi.int] size]
	puts [[ffi.long] size]
	puts [[ffi.pointer] size]
}

proc types_example {} {
	set count [ffi.int 12345678]
	puts "integer with contents [$count value] is at [$count address]"

	# the raw contents of a variable may be accessed using "raw"
	puts "the raw, [$count size] bytes form of [$count value] is [$count raw]"

	# pointers are created by calling ffi.pointer with the address of a variable
	set count_ptr [ffi.pointer [$count address]]
	puts [$count_ptr value]

	# strings are created using "ffi.string copy" or "ffi.string at"
	set s [ffi.string copy "this is a string"]
	puts [$s value]
}

proc simple_example {} {
	# locate sprintf() and define its prototype
	set sprintf_ptr [$::libc int sprintf string string string int]

	# create objects for the arguments of sprintf() - they should match the
	# prototype
	set buf [ffi.buffer 32]
	set fmt [ffi.string copy "%s %d"]
	set arg1 [ffi.string copy "aha"]
	set arg2 [ffi.int 1337]

	# create an object for the return value; if no value is specified it's an
	# uninitialized variable
	set ret [ffi.int]

	# call sprintf() - variables must be passed as pointers
	$sprintf_ptr [$ret address] "[$buf address] [$fmt address] [$arg1 address] [$arg2 address]"

	# print the output buffer (the first argument)
	puts [$buf value]

	# print the return value of sprintf()
	puts [$ret value]
}


proc pointers_example {} {
	# call time() to get the current time
	set time_ptr [$::libc void time pointer]
	set now [ffi.int]
	$time_ptr [[ffi.void] address] [[ffi.pointer [$now address]] address]

	# call gmtime(), which returns a struct tm pointer
	set gmtime_ptr [$::libc pointer gmtime pointer]
	set now_ptr [ffi.pointer [$now address]]
	set now_broken [ffi.pointer]
	$gmtime_ptr [$now_broken address] [$now_ptr address]

	# call asctime() and pass the struct tm pointer
	set asctime_ptr [$::libc pointer asctime pointer]
	set now_broken_ptr [ffi.pointer [$now_broken value]]
	set out [ffi.pointer]
	$asctime_ptr [$out address] [$now_broken_ptr address]

	# asctime() returns a string - print it
	puts [ffi.string at [$out value] 24]
}

proc structs_example {} {
	# locate asctime()
	set asctime_ptr [$::libc pointer asctime pointer]

	# create a struct tm and initialize it with January 30th 1992, 2:10 AM
	set now_broken [ffi.struct "[[ffi.int 0] raw][[ffi.int 10] raw][[ffi.int 2] raw][[ffi.int 30] raw][[ffi.int 0] raw][[ffi.int 92] raw][[ffi.int 0] raw][[ffi.int 30] raw][[ffi.int 0] raw][[ffi.long 0] raw][[ffi.pointer 0] raw]" int int int int int int int int int long pointer]

	# for demonstration purposes, read tm_year (the 6th member of struct tm),
	# using "member" and the zero-based member index
	set tm_year 5
	puts [[$now_broken member $tm_year] value]

	# call asctime()
	set now_broken_ptr [ffi.pointer [$now_broken address]]
	set out [ffi.pointer]
	$asctime_ptr [$out address] [$now_broken_ptr address]
	puts [ffi.string at [$out value] 24]
}

sizeof_example
types_example
simple_example
pointers_example
structs_example
