# one of Mark's test cases for line numbering in stack trace after error.

proc a {} {
    b
}

proc b {} {
    for {set i 0} {$i < 5} {incr i} {
        bad command here
    }
}    

puts started
a
puts ended

