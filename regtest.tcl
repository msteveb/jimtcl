# REGTEST 1
# 27Jan2005 - SIGSEGV for bug on Jim_DuplicateObj().

for {set i 0} {$i < 100} {incr i} {
    set a "x"
    lappend a n
}
puts "TEST 1 PASSED"

# REGTEST 2
# 29Jan2005 - SEGFAULT parsing script composed of just one comment.
eval {#foobar}
puts "TEST 2 PASSED"

# REGTEST 3
# 29Jan2005 - "Error in Expression" with correct expression
set x 5
expr {$x-5}
puts "TEST 3 PASSED"

# REGTEST 4
# 29Jan2005 - SIGSEGV when run this code, due to expr's bug.
proc fibonacci {x} {
    if {$x <= 1} {
    expr 1
    } else {
    expr {[fibonacci [expr {$x-1}]] + [fibonacci [expr {$x-2}]]}
    }
}
fibonacci 6
puts "TEST 4 PASSED"

# REGTEST 5
# 06Mar2005 - This looped forever...
for {set i 0} {$i < 10} {incr i} {continue}
puts "TEST 5 PASSED"

# REGTEST 6
# 07Mar2005 - Unset create variable + dict is using dict syntax sugar at
#             currently non-existing variable
catch {unset thisvardoesnotexists(thiskeytoo)}
if {[catch {set thisvardoesnotexists}] == 0} {
  puts "TEST 6 FAILED - unset created dict for non-existing variable"
  break
}
puts "TEST 6 PASSED"

# TAKE THE FOLLOWING puts AS LAST LINE

puts "--- ALL TESTS PASSED ---"
