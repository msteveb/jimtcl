# These regression tests all provoked crashes at some point.
# Thus they are kept separate from the regular test suite in tests/

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

# REGTEST 7
# 04Nov2008 - variable parsing does not eat last brace
set a 1
list ${a}
puts "TEST 7 PASSED"

# REGTEST 8
# 04Nov2008 - string toupper/tolower do not convert to string rep
string tolower [list a]
string toupper [list a]
puts "TEST 8 PASSED"

# REGTEST 9
# 04Nov2008 - crash on exit when replacing Tcl proc with C command.
# Requires the clock extension to be built as a loadable module.
proc clock {args} {}
catch {package require clock}
# Note, crash on exit, so don't say we passed!

# REGTEST 10
# 05Nov2008 - incorrect lazy expression evaluation with unary not
expr {1 || !0}
puts "TEST 10 PASSED"

# REGTEST 11
# 14 Feb 2010 - access static variable in deleted proc
proc a {} {{x 1}} { rename a ""; incr x }
a
puts "TEST 11 PASSED"

# REGTEST 12
# 13 Sep 2010 - reference with invalid tag
set a b[ref value "tag name"]
getref [string range $a 1 end]
puts "TEST 12 PASSED"

# REGTEST 13
# 14 Sep 2010 - parse list with trailing backslash
set x "switch -0 \$on \\"
lindex $x 1
puts "TEST 13 PASSED"

# REGTEST 14
# 14 Sep 2010 - command expands to nothing
eval "{*}{}"
puts "TEST 14 PASSED"

# REGTEST 15
# 24 Feb 2010 - bad reference counting of the stack trace in 'error'
proc a {msg stack} {
    tailcall error $msg $stack
}
catch {fail} msg opts
catch {a $msg $opts(-errorinfo)}

# REGTEST 16
# 24 Feb 2010 - rename the current proc
# Leaves unfreed objects on the stack
proc a {} { rename a newa}
a

# REGTEST 17
# 26 Nov 2010 - crashes on invalid dict sugar
catch {eval {$x(}}
puts "TEST 17 PASSED"

# REGTEST 18
# 12 Apr 2011 - crashes on unset for loop var
catch {
    set j 0
    for {set i 0} {$i < 5} {incr i} {
        unset i
        if {[incr j] == 5} {
            break
        }
    }
}
puts "TEST 18 PASSED"

# REGTEST 19
# 25 May 2011 - crashes with double colon
catch {
    expr {5 ne ::}
}
puts "TEST 19 PASSED"

# REGTEST 20
# 26 May 2011 - infinite recursion
proc a {} { global ::blah; set ::blah test }
a
puts "TEST 20 PASSED"

# REGTEST 21
# 26 May 2011 - infinite loop with null byte in subst
subst "abc\0def"
puts "TEST 21 PASSED"

# REGTEST 22
# 21 June 2011 - crashes on lappend to to value with script rep
set x rand
eval $x
lappend x b
puts "TEST 22 PASSED"

# REGTEST 23
# 27 July 2011 - unfreed objects on exit
catch {
    set x abc
    subst $x
    regexp $x $x
}
# Actually, the test passes if no objects leaked on exit
puts "TEST 23 PASSED"

# REGTEST 24
# 13 Nov 2011 - invalid cached global var
proc a {} {
    foreach i {1 2} {
        incr z [set ::t]
        unset ::t
    }
}
set t 6
catch a
puts "TEST 24 PASSED"

# REGTEST 25
# 14 Nov 2011 - link global var to proc var
proc a {} {
    set x 3
    upvar 0 x ::globx
}
set globx 0
catch {
    a
}
incr globx
puts "TEST 25 PASSED"

# REGTEST 26
# 2 Dec 2011 - infinite eval recursion
catch {
    set x 0
    set y {incr x; eval $y}
    eval $y
} msg
puts "TEST 26 PASSED"

# REGTEST 27
# 2 Dec 2011 - infinite alias recursion
catch {
    proc p {} {}
    alias p p
    p
} msg
puts "TEST 27 PASSED"

# REGTEST 28
# 16 Dec 2011 - ref count problem with finalizers
catch {
    ref x x [list dummy]
    collect
}
puts "TEST 28 PASSED"

# REGTEST 29
# Reference counting problem at exit
set x [lindex {} 0]
info source $x
eval $x
puts "TEST 29 PASSED"

# REGTEST 30
# non-UTF8 string tolower 
string tolower "/mod/video/h\303\203\302\244xan_ witchcraft through the ages_20131101_0110.t"
puts "TEST 30 PASSED"

# REGTEST 31
# infinite lsort -unique with error
catch {lsort -unique -real {foo 42.0}}
puts "TEST 31 PASSED"

# REGTEST 32
# return -code eval should only used by tailcall, but this incorrect usage
# should not crash the interpreter
proc a {} { tailcall b }
proc b {} { return -code eval c }
proc c {} {}
catch -eval a
puts "TEST 32 PASSED"

# REGTEST 33
# unset array variable which doesn't exist
array unset blahblah abc
puts "TEST 33 PASSED"

# REGTEST 34
# onexception and writable conflict
set f [open [info nameofexecutable]]
$f onexception {incr x}
$f writable {incr y}
$f close
puts "TEST 34 PASSED"

# TAKE THE FOLLOWING puts AS LAST LINE

puts "--- ALL TESTS PASSED ---"
