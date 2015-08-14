package require ffi

# load the C libary
set libc [ffi.dlopen libc.so.6]

# locate sprintf() and define its prototype
set sprintf [$libc int sprintf string string string int]

# create objects for the arguments of sprintf() - they should match the
# prototype
set buf [ffi.buffer 32]
set fmt [ffi.string "%s %d"]
set arg1 [ffi.string "aha"]
set arg2 [ffi.int 1337]

# create an object for the return value; if no value is specified it's an
# uninitialized variable
set ret [ffi.int]

# call sprintf() - variables must be passed as pointers
$sprintf [$ret pointer] "[$buf pointer] [$fmt pointer] [$arg1 pointer] [$arg2 pointer]"

# print the output buffer (the first argument)
puts [$buf value]

# print the return value of sprintf()
puts [$ret value]
