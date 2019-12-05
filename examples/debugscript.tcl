proc a {} {
    b
}
  
proc b {} {
    debugscript begin
    eval {
        bad command here looooooooooooooooooooooooooooooooooooong
    }
}    

puts started
debugscript begin
puts "stepping now"
a
puts ended
