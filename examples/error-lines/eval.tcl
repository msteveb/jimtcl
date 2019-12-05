# one of Mark's test cases for line numbering in stack trace after error.

proc a {} {
    b
}
  
proc b {} {
    eval {
        bad command here
    }
}    

puts started
a
puts ended

