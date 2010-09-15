# Example of using the 'putter' function to redirect parray output

set a {1 one 2 two 3 three}

# Use 'curry' to create a single command from two words
stderr puts "curry"
parray a * [curry stderr puts]

# Same thing, but an alias instead
stderr puts "\nalias"
alias stderr_puts stderr puts
parray a * stderr_puts

# Now use a lambda to accumulate the results in a buffer
stderr puts "\nlamba"
parray a * [lambda {msg} {lappend ::lines $msg}]
stderr puts [join $lines \n]
