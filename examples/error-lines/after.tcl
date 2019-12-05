# one of Mark's test cases for line numbering in stack trace after error.

proc a {} {
    b
}
  
proc b {} {
    after 200 {
        bad command here
    }
}    

puts started
a
vwait junk
puts ended

