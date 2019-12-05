# one of Mark's test cases for line numbering in stack trace after error.

proc a {} {
    b
}

proc b {} {
    loop i 0 5 {
        bad command here
    }
}    

puts started
a
puts ended

