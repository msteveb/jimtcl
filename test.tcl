# $Id: test.tcl,v 1.21 2005/03/11 09:31:36 antirez Exp $
#
# This are Tcl tests imported into Jim. Tests that will probably not be passed
# in the long term are usually removed (for example all the tests about
# unicode things, about errors in list parsing that are always valid in Jim
# and so on).
#
# Sometimes tests are modified to reflect different error messages.

set failedTests 0
set passedTests 0

proc test {id descr script expectedResult} {
    global failedTests passedTests

    puts -nonewline "$id $descr: "
    set result [uplevel 1 $script]
    if {$result eq $expectedResult} {
	puts "OK"
	incr passedTests
    } else {
	puts "ERR"
	puts "Expected: '$expectedResult'"
	puts "Got     : '$result'"
	incr failedTests
    }
}

proc error {msg} { return -code error $msg }

################################################################################
# JIM REGRESSION TESTS
################################################################################
test regression-1.0 {Rename against procedures with static vars} {
    proc foobar {x} {{y 10}} {
        incr y $x
    }
    foobar 30
    foobar 20
    rename foobar barfoo
    list [barfoo 1] [barfoo 2] [barfoo 3]
} {61 63 66}

rename barfoo {}

################################################################################
# SET
################################################################################

test set-1.2 {TclCompileSetCmd: simple variable name} {
    set i 10
    list [set i] $i
} {10 10}

test set-1.4 {TclCompileSetCmd: simple variable name in quotes} {
    set i 17
    list [set "i"] $i
} {17 17}

test set-1.7 {TclCompileSetCmd: non-simple (computed) variable name} {
    set x "i"
    set i 77
    list [set $x] $i
} {77 77}

test set-1.8 {TclCompileSetCmd: non-simple (computed) variable name} {
    set x "i"
    set i 77
    list [set [set x] 2] $i
} {2 2}

test set-1.9 {TclCompileSetCmd: 3rd arg => assignment} {
    set i "abcdef"
    list [set i] $i
} {abcdef abcdef}

test set-1.10 {TclCompileSetCmd: only two args => just getting value} {
    set i {one two}
    set i
} {one two}

test set-1.11 {TclCompileSetCmd: simple global name} {
    proc p {} {
        global i
        set i 54
        set i
    }
    p
} {54}

test set-1.12 {TclCompileSetCmd: simple local name} {
    proc p {bar} {
        set foo $bar
        set foo
    }
    p 999
} {999}

test set-1.14 {TclCompileSetCmd: simple local name, >255 locals} {
    proc 260locals {} {
        # create 260 locals (the last ones with index > 255)
        set a0 0; set a1 0; set a2 0; set a3 0; set a4 0
        set a5 0; set a6 0; set a7 0; set a8 0; set a9 0
        set b0 0; set b1 0; set b2 0; set b3 0; set b4 0
        set b5 0; set b6 0; set b7 0; set b8 0; set b9 0
        set c0 0; set c1 0; set c2 0; set c3 0; set c4 0
        set c5 0; set c6 0; set c7 0; set c8 0; set c9 0
        set d0 0; set d1 0; set d2 0; set d3 0; set d4 0
        set d5 0; set d6 0; set d7 0; set d8 0; set d9 0
        set e0 0; set e1 0; set e2 0; set e3 0; set e4 0
        set e5 0; set e6 0; set e7 0; set e8 0; set e9 0
        set f0 0; set f1 0; set f2 0; set f3 0; set f4 0
        set f5 0; set f6 0; set f7 0; set f8 0; set f9 0
        set g0 0; set g1 0; set g2 0; set g3 0; set g4 0
        set g5 0; set g6 0; set g7 0; set g8 0; set g9 0
        set h0 0; set h1 0; set h2 0; set h3 0; set h4 0
        set h5 0; set h6 0; set h7 0; set h8 0; set h9 0
        set i0 0; set i1 0; set i2 0; set i3 0; set i4 0
        set i5 0; set i6 0; set i7 0; set i8 0; set i9 0
        set j0 0; set j1 0; set j2 0; set j3 0; set j4 0
        set j5 0; set j6 0; set j7 0; set j8 0; set j9 0
        set k0 0; set k1 0; set k2 0; set k3 0; set k4 0
        set k5 0; set k6 0; set k7 0; set k8 0; set k9 0
        set l0 0; set l1 0; set l2 0; set l3 0; set l4 0
        set l5 0; set l6 0; set l7 0; set l8 0; set l9 0
        set m0 0; set m1 0; set m2 0; set m3 0; set m4 0
        set m5 0; set m6 0; set m7 0; set m8 0; set m9 0
        set n0 0; set n1 0; set n2 0; set n3 0; set n4 0
        set n5 0; set n6 0; set n7 0; set n8 0; set n9 0
        set o0 0; set o1 0; set o2 0; set o3 0; set o4 0
        set o5 0; set o6 0; set o7 0; set o8 0; set o9 0
        set p0 0; set p1 0; set p2 0; set p3 0; set p4 0
        set p5 0; set p6 0; set p7 0; set p8 0; set p9 0
        set q0 0; set q1 0; set q2 0; set q3 0; set q4 0
        set q5 0; set q6 0; set q7 0; set q8 0; set q9 0
        set r0 0; set r1 0; set r2 0; set r3 0; set r4 0
        set r5 0; set r6 0; set r7 0; set r8 0; set r9 0
        set s0 0; set s1 0; set s2 0; set s3 0; set s4 0
        set s5 0; set s6 0; set s7 0; set s8 0; set s9 0
        set t0 0; set t1 0; set t2 0; set t3 0; set t4 0
        set t5 0; set t6 0; set t7 0; set t8 0; set t9 0
        set u0 0; set u1 0; set u2 0; set u3 0; set u4 0
        set u5 0; set u6 0; set u7 0; set u8 0; set u9 0
        set v0 0; set v1 0; set v2 0; set v3 0; set v4 0
        set v5 0; set v6 0; set v7 0; set v8 0; set v9 0
        set w0 0; set w1 0; set w2 0; set w3 0; set w4 0
        set w5 0; set w6 0; set w7 0; set w8 0; set w9 0
        set x0 0; set x1 0; set x2 0; set x3 0; set x4 0
        set x5 0; set x6 0; set x7 0; set x8 0; set x9 0
        set y0 0; set y1 0; set y2 0; set y3 0; set y4 0
        set y5 0; set y6 0; set y7 0; set y8 0; set y9 0
        set z0 0; set z1 0; set z2 0; set z3 0; set z4 0
        set z5 0; set z6 0; set z7 0; set z8 0; set z9 1234
    }
    260locals
} {1234}

test set-1.17 {TclCompileSetCmd: doing assignment, simple int} {
    set i 5
    set i 123
} 123

test set-1.18 {TclCompileSetCmd: doing assignment, simple int} {
    set i 5
    set i -100
} -100

test set-1.19 {TclCompileSetCmd: doing assignment, simple but not int} {
    set i 5
    set i 0x12MNOP
    set i
} {0x12MNOP}

test set-1.20 {TclCompileSetCmd: doing assignment, in quotes} {
    set i 25
    set i "-100"
} -100

test set-1.21 {TclCompileSetCmd: doing assignment, in braces} {
    set i 24
    set i {126}
} 126

test set-1.22 {TclCompileSetCmd: doing assignment, large int} {
    set i 5
    set i 200000
} 200000

test set-1.23 {TclCompileSetCmd: doing assignment, formatted int != int} {
    set i 25
    set i 000012345     ;# an octal literal == 5349 decimal
    list $i [incr i]
} {000012345 5350}

################################################################################
# LIST
################################################################################

test list-1.1 {basic tests} {list a b c} {a b c}
test list-1.2 {basic tests} {list {a b} c} {{a b} c}
test list-1.3 {basic tests} {list \{a b c} {\{a b c}
test list-1.4 {basic tests} "list a{}} b{} c}" "a\\{\\}\\} b{} c\\}"
test list-1.5 {basic tests} {list a\[ b\] } "{a\[} b\\]"
test list-1.6 {basic tests} {list c\  d\t } "{c } {d\t}"
test list-1.7 {basic tests} {list e\n f\$ } "{e\n} {f\$}"
test list-1.8 {basic tests} {list g\; h\\} {{g;} h\\}
test list-1.9 {basic tests} "list a\\\[} b\\\]} " "a\\\[\\\} b\\\]\\\}"
test list-1.10 {basic tests} "list c\\\} d\\t} " "c\\} d\\t\\}"
test list-1.11 {basic tests} "list e\\n} f\\$} " "e\\n\\} f\\$\\}"
test list-1.12 {basic tests} "list g\\;} h\\\\} " "g\\;\\} {h\\}}"
test list-1.13 {basic tests} {list a {{}} b} {a {{}} b}
test list-1.14 {basic tests} {list a b xy\\} "a b xy\\\\"
test list-1.15 {basic tests} "list a b\} e\\" "a b\\} e\\\\"
test list-1.16 {basic tests} "list a b\}\\\$ e\\\$\\" "a b\\}\\\$ e\\\$\\\\"
test list-1.17 {basic tests} {list a\f \{\f} "{a\f} \\\{\\f"
test list-1.18 {basic tests} {list a\r \{\r} "{a\r} \\\{\\r"
test list-1.19 {basic tests} {list a\v \{\v} "{a\v} \\\{\\v"
test list-1.20 {basic tests} {list \"\}\{} "\\\"\\}\\{"
test list-1.21 {basic tests} {list a b c\\\nd} "a b c\\\\\\nd"
test list-1.22 {basic tests} {list "{ab}\\"} \\{ab\\}\\\\
test list-1.23 {basic tests} {list \{} "\\{"
test list-1.24 {basic tests} {list} {}

set num 0
proc lcheck {testid a b c} {
    global num d
    set d [list $a $b $c]
    test ${testid}-0 {what goes in must come out} {lindex $d 0} $a
    test ${testid}-1 {what goes in must come out} {lindex $d 1} $b
    test ${testid}-2 {what goes in must come out} {lindex $d 2} $c
}
lcheck list-2.1  a b c
lcheck list-2.2  "a b" c\td e\nf
lcheck list-2.3  {{a b}} {} {  }
lcheck list-2.4  \$ \$ab ab\$
lcheck list-2.5  \; \;ab ab\;
lcheck list-2.6  \[ \[ab ab\[
lcheck list-2.7  \\ \\ab ab\\
lcheck list-2.8  {"} {"ab} {ab"}        ;#" Stupid emacs highlighting!
lcheck list-2.9  {a b} { ab} {ab }
lcheck list-2.10 a{ a{b \{ab
lcheck list-2.11 a} a}b }ab
lcheck list-2.12 a\\} {a \}b} {a \{c}
lcheck list-2.13 xyz \\ 1\\\n2
lcheck list-2.14 "{ab}\\" "{ab}xy" abc

concat {}

################################################################################
# WHILE
################################################################################

test while-1.9 {TclCompileWhileCmd: simple command body} {
    set a {}
    set i 1
    while {$i<6} {
        if $i==4 break
        set a [concat $a $i]
        incr i
    }
    set a
} {1 2 3}

test while-1.10 {TclCompileWhileCmd: command body in quotes} {
    set a {}
    set i 1
    while {$i<6} "append a x; incr i"
    set a
} {xxxxx}

test while-1.13 {TclCompileWhileCmd: while command result} {
    set i 0
    set a [while {$i < 5} {incr i}]
    set a
} {}

test while-1.14 {TclCompileWhileCmd: while command result} {
    set i 0
    set a [while {$i < 5} {if $i==3 break; incr i}]
    set a
} {}

test while-2.1 {continue tests} {
    set a {}
    set i 1
    while {$i <= 4} {
        incr i
        if {$i == 3} continue
        set a [concat $a $i]
    }
    set a
} {2 4 5}
test while-2.2 {continue tests} {
    set a {}
    set i 1
    while {$i <= 4} {
        incr i
        if {$i != 2} continue
        set a [concat $a $i]
    }
    set a
} {2}
test while-2.3 {continue tests, nested loops} {
    set msg {}
    set i 1
    while {$i <= 4} {
        incr i
        set a 1
        while {$a <= 2} {
            incr a
            if {$i>=3 && $a>=3} continue
            set msg [concat $msg "$i.$a"]
        }
    }
    set msg
} {2.2 2.3 3.2 4.2 5.2}

test while-4.1 {while and computed command names} {
    set i 0
    set z while
    $z {$i < 10} {
        incr i
    }
    set i
} 10

test while-5.2 {break tests with computed command names} {
    set a {}
    set i 1
    set z break
    while {$i <= 4} {
        if {$i == 3} $z
        set a [concat $a $i]
        incr i
    }
    set a
} {1 2}

test while-7.1 {delayed substitution of body} {
    set i 0
    while {[incr i] < 10} "
       set result $i
    "
    proc p {} {
        set i 0
        while {[incr i] < 10} "
            set result $i
        "
        set result
    }
    append result [p]
} {00}

################################################################################
# LSET
################################################################################

set lset lset

test lset-2.1 {lset, not compiled, 3 args, second arg a plain index} {
    set x {0 1 2}
    list [eval [list $lset x 0 3]] $x
} {{3 1 2} {3 1 2}}

test lset-3.1 {lset, not compiled, 3 args, data duplicated} {
    set x {0 1 2}
    list [eval [list $lset x 0 $x]] $x
} {{{0 1 2} 1 2} {{0 1 2} 1 2}}

test lset-3.2 {lset, not compiled, 3 args, data duplicated} {
    set x {0 1}
    set y $x
    list [eval [list $lset x 0 2]] $x $y
} {{2 1} {2 1} {0 1}}

test lset-3.3 {lset, not compiled, 3 args, data duplicated} {
    set x {0 1}
    set y $x
    list [eval [list $lset x 0 $x]] $x $y
} {{{0 1} 1} {{0 1} 1} {0 1}}

test lset-3.4 {lset, not compiled, 3 args, data duplicated} {
    set x {0 1 2}
    list [eval [list $lset x [list 0] $x]] $x
} {{{0 1 2} 1 2} {{0 1 2} 1 2}}

test lset-3.5 {lset, not compiled, 3 args, data duplicated} {
    set x {0 1}
    set y $x
    list [eval [list $lset x [list 0] 2]] $x $y
} {{2 1} {2 1} {0 1}}

test lset-3.6 {lset, not compiled, 3 args, data duplicated} {
    set x {0 1}
    set y $x
    list [eval [list $lset x [list 0] $x]] $x $y
} {{{0 1} 1} {{0 1} 1} {0 1}}

test lset-4.2 {lset, not compiled, 3 args, bad index} {
    set a {x y z}
    list [catch {
	eval [list $lset a [list 2a2] w]
    } msg] $msg
} {1 {bad index "2a2": must be integer or end?-integer?}}

test lset-4.3 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a [list -1] w]
    } msg] $msg
} {1 {list index out of range}}

test lset-4.4 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a [list 3] w]
    } msg] $msg
} {1 {list index out of range}}

test lset-4.5 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a [list end--1] w]
    } msg] $msg
} {1 {list index out of range}}

test lset-4.6 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a [list end-3] w]
    } msg] $msg
} {1 {list index out of range}}

test lset-4.8 {lset, not compiled, 3 args, bad index} {
    set a {x y z}
    list [catch {
	eval [list $lset a 2a2 w]
    } msg] $msg
} {1 {bad index "2a2": must be integer or end?-integer?}}

test lset-4.9 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a -1 w]
    } msg] $msg
} {1 {list index out of range}}

test lset-4.10 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a 3 w]
    } msg] $msg
} {1 {list index out of range}}

test lset-4.11 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a end--1 w]
    } msg] $msg
} {1 {list index out of range}}

test lset-4.12 {lset, not compiled, 3 args, index out of range} {
    set a {x y z}
    list [catch {
	eval [list $lset a end-3 w]
    } msg] $msg
} {1 {list index out of range}}

test lset-6.1 {lset, not compiled, 3 args, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a 0 a]] $a
} {{a y z} {a y z}}

test lset-6.2 {lset, not compiled, 3 args, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a [list 0] a]] $a
} {{a y z} {a y z}}

test lset-6.3 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a 2 a]] $a
} {{x y a} {x y a}}

test lset-6.4 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a [list 2] a]] $a
} {{x y a} {x y a}}

test lset-6.5 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a end a]] $a
} {{x y a} {x y a}}

test lset-6.6 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a [list end] a]] $a
} {{x y a} {x y a}}

test lset-6.7 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a end-0 a]] $a
} {{x y a} {x y a}}

test lset-6.8 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a [list end-0] a]] $a
} {{x y a} {x y a}}
test lset-6.9 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a end-2 a]] $a
} {{a y z} {a y z}}

test lset-6.10 {lset, not compiled, 1-d list basics} {
    set a {x y z}
    list [eval [list $lset a [list end-2] a]] $a
} {{a y z} {a y z}}

test lset-7.1 {lset, not compiled, data sharing} {
    set a 0
    list [eval [list $lset a $a {gag me}]] $a
} {{{gag me}} {{gag me}}}

test lset-7.2 {lset, not compiled, data sharing} {
    set a [list 0]
    list [eval [list $lset a $a {gag me}]] $a
} {{{gag me}} {{gag me}}}

test lset-7.3 {lset, not compiled, data sharing} {
    set a {x y}
    list [eval [list $lset a 0 $a]] $a
} {{{x y} y} {{x y} y}}

test lset-7.4 {lset, not compiled, data sharing} {
    set a {x y}
    list [eval [list $lset a [list 0] $a]] $a
} {{{x y} y} {{x y} y}}

test lset-7.5 {lset, not compiled, data sharing} {
    set n 0
    set a {x y}
    list [eval [list $lset a $n $n]] $a $n
} {{0 y} {0 y} 0}

test lset-7.6 {lset, not compiled, data sharing} {
    set n [list 0]
    set a {x y}
    list [eval [list $lset a $n $n]] $a $n
} {{0 y} {0 y} 0}

test lset-7.7 {lset, not compiled, data sharing} {
    set n 0
    set a [list $n $n]
    list [eval [list $lset a $n 1]] $a $n
} {{1 0} {1 0} 0}

test lset-7.8 {lset, not compiled, data sharing} {
    set n [list 0]
    set a [list $n $n]
    list [eval [list $lset a $n 1]] $a $n
} {{1 0} {1 0} 0}

test lset-7.9 {lset, not compiled, data sharing} {
    set a 0
    list [eval [list $lset a $a $a]] $a
} {0 0}

test lset-7.10 {lset, not compiled, data sharing} {
    set a [list 0]
    list [eval [list $lset a $a $a]] $a
} {0 0}

test lset-8.3 {lset, not compiled, bad second index} {
    set a {{b c} {d e}}
    list [catch {eval [list $lset a 0 2a2 f]} msg] $msg
} {1 {bad index "2a2": must be integer or end?-integer?}}

test lset-8.5 {lset, not compiled, second index out of range} {
    set a {{b c} {d e} {f g}}
    list [catch {eval [list $lset a 2 -1 h]} msg] $msg
} {1 {list index out of range}}

test lset-8.7 {lset, not compiled, second index out of range} {
    set a {{b c} {d e} {f g}}
    list [catch {eval [list $lset a 2 2 h]} msg] $msg
} {1 {list index out of range}}

test lset-8.9 {lset, not compiled, second index out of range} {
    set a {{b c} {d e} {f g}}
    list [catch {eval [list $lset a 2 end--1 h]} msg] $msg
} {1 {list index out of range}}

test lset-8.11 {lset, not compiled, second index out of range} {
    set a {{b c} {d e} {f g}}
    list [catch {eval [list $lset a 2 end-2 h]} msg] $msg
} {1 {list index out of range}}

test lset-9.1 {lset, not compiled, entire variable} {
    set a x
    list [eval [list $lset a y]] $a
} {y y}

test lset-10.1 {lset, not compiled, shared data} {
    set row {p q}
    set a [list $row $row]
    list [eval [list $lset a 0 0 x]] $a
} {{{x q} {p q}} {{x q} {p q}}}

test lset-11.1 {lset, not compiled, 2-d basics} {
    set a {{b c} {d e}}
    list [eval [list $lset a 0 0 f]] $a
} {{{f c} {d e}} {{f c} {d e}}}

test lset-11.3 {lset, not compiled, 2-d basics} {
    set a {{b c} {d e}}
    list [eval [list $lset a 0 1 f]] $a
} {{{b f} {d e}} {{b f} {d e}}}

test lset-11.5 {lset, not compiled, 2-d basics} {
    set a {{b c} {d e}}
    list [eval [list $lset a 1 0 f]] $a
} {{{b c} {f e}} {{b c} {f e}}}

test lset-11.7 {lset, not compiled, 2-d basics} {
    set a {{b c} {d e}}
    list [eval [list $lset a 1 1 f]] $a
} {{{b c} {d f}} {{b c} {d f}}}

test lset-12.0 {lset, not compiled, typical sharing pattern} {
    set zero 0
    set row [list $zero $zero $zero $zero]
    set ident [list $row $row $row $row]
    for { set i 0 } { $i < 4 } { incr i } {
	eval [list $lset ident $i $i 1]
    }
    set ident
} {{1 0 0 0} {0 1 0 0} {0 0 1 0} {0 0 0 1}}

test lset-13.0 {lset, not compiled, shimmering hell} {
    set a 0
    list [eval [list $lset a $a $a $a $a {gag me}]] $a
} {{{{{{gag me}}}}} {{{{{gag me}}}}}}

test lset-13.1 {lset, not compiled, shimmering hell} {
    set a [list 0]
    list [eval [list $lset a $a $a $a $a {gag me}]] $a
} {{{{{{gag me}}}}} {{{{{gag me}}}}}}

test lset-14.1 {lset, not compiled, list args, is string rep preserved?} {
    set a { { 1 2 } { 3 4 } }
    catch { eval [list $lset a {1 5} 5] }
    list $a [lindex $a 1]
} "{ { 1 2 } { 3 4 } } { 3 4 }"

catch {unset noRead}
catch {unset noWrite}
catch {rename failTrace {}}
catch {unset ::x}
catch {unset ::y}

################################################################################
# IF
################################################################################

test if-1.1 {bad syntax: lacking all} {
	catch {if}
} 1
test if-1.2 {bad syntax: lacking then-clause} {
	catch {if 1==1}
} 1
test if-1.3 {bad syntax: lacking then-clause 2} {
	catch {if 1==1 then}
} 1
test if-1.4 {bad syntax: lacking else-clause after keyword 'else'} {
	catch {if 1==0 then {list 1} else}
} 1
test if-1.5 {bad syntax: lacking expr after 'elseif'} {
	catch {if 1==0 then {list 1} elseif}
} 1
test if-1.6 {bad syntax: lacking then-clause after 'elseif'} {
	catch {if 1==0 then {list 1} elseif 1==1}
} 1
test if-1.7 {bad syntax: lacking else-clause after 'elseif' after keyword 'else'} {
	catch {if 1==0 then {list 1} elseif 1==0 {list 2} else}
} 1
test if-1.8 {bad syntax: extra arg after implicit else-clause} {
	catch {if 1==0 {list 1} elseif 1==0 then {list 2} {list 3} else}
} 1
test if-1.9 {bad syntax: elsif-clause after else-clause} {
	catch {if 1==0 {list 1} else {list 2} elseif 1==1 {list 3}}
} 1
test if-2.1 {taking proper branch} {
    set a {}
    if 0 {set a 1} else {set a 2}
    set a
} 2
test if-2.2 {taking proper branch} {
    set a {}
    if 1 {set a 1} else {set a 2}
    set a
} 1
test if-2.3 {taking proper branch} {
    set a {}
    if 1<2 {set a 1}
    set a
} 1
test if-2.4 {taking proper branch} {
    set a {}
    if 1>2 {set a 1}
    set a
} {}
test if-2.5 {taking proper branch} {
    set a {}
    if 0 {set a 1} else {}
    set a
} {}
test if-2.6 {taking proper branch} {
    set a {}
    if 0 {set a 1} elseif 1 {set a 2} elseif 1 {set a 3} else {set a 4}
    set a
} 2
test if-2.7 {taking proper branch} {
    set a {}
    if 0 {set a 1} elseif 0 {set a 2} elseif 1 {set a 3} else {set a 4}
    set a
} 3
test if-2.8 {taking proper branch} {
    set a {}
    if 0 {set a 1} elseif 0 {set a 2} elseif 0 {set a 3} else {set a 4}
    set a
} 4
test if-2.9 {taking proper branch, multiline test expr} {
    set a {}
    if {1 != \
	     3} {set a 3} else {set a 4}
    set a
} 3
test if-3.1 {optional then-else args} {
    set a 44
    if 0 then {set a 1} elseif 0 then {set a 3} else {set a 2}
    set a
} 2
test if-3.2 {optional then-else args} {
    set a 44
    if 1 then {set a 1} else {set a 2}
    set a
} 1
test if-3.3 {optional then-else args} {
    set a 44
    if 0 {set a 1} else {set a 2}
    set a
} 2
test if-3.4 {optional then-else args} {
    set a 44
    if 1 {set a 1} else {set a 2}
    set a
} 1
test if-3.5 {optional then-else args} {
    set a 44
    if 0 then {set a 1} {set a 2}
    set a
} 2
test if-3.6 {optional then-else args} {
    set a 44
    if 1 then {set a 1} {set a 2}
    set a
} 1
test if-3.7 {optional then-else args} {
    set a 44
    if 0 then {set a 1} else {set a 2}
    set a
} 2
test if-3.8 {optional then-else args} {
    set a 44
    if 0 then {set a 1} elseif 0 {set a 2} elseif 0 {set a 3} {set a 4}
    set a
} 4
test if-4.1 {return value} {
    if 1 then {set a 22; concat abc}
} abc
test if-4.2 {return value} {
    if 0 then {set a 22; concat abc} elseif 1 {concat def} {concat ghi}
} def
test if-4.3 {return value} {
    if 0 then {set a 22; concat abc} else {concat def}
} def
test if-4.4 {return value} {
    if 0 then {set a 22; concat abc}
} {}
test if-4.5 {return value} {
    if 0 then {set a 22; concat abc} elseif 0 {concat def}
} {}
test if-5.1 {error conditions} {
    list [catch {if {[error "error in condition"]} foo} msg] $msg
} {1 {error in condition}}
test if-5.2 {error conditions} {
    list [catch {if 2 the} msg] $msg
} {1 {invalid command name "the"}}
test if-5.3 {error conditions} {
    list [catch {if 2 then {[error "error in then clause"]}} msg] $msg
} {1 {error in then clause}}
test if-5.4 {error conditions} {
    list [catch {if 0 then foo elsei} msg] $msg
} {1 {invalid command name "elsei"}}
test if-5.5 {error conditions} {
    list [catch {if 0 then foo elseif 0 bar els} msg] $msg
} {1 {invalid command name "els"}}
test if-5.6 {error conditions} {
    list [catch {if 0 then foo elseif 0 bar else {[error "error in else clause"]}} msg] $msg
} {1 {error in else clause}}

################################################################################
# APPEND
################################################################################

catch {unset x}

test append-1.1 {append command} {
    catch {unset x}
    list [append x 1 2 abc "long string"] $x
} {{12abclong string} {12abclong string}}
test append-1.2 {append command} {
    set x ""
    list [append x first] [append x second] [append x third] $x
} {first firstsecond firstsecondthird firstsecondthird}
test append-1.3 {append command} {
    set x "abcd"
    append x
} abcd

test append-2.1 {long appends} {
    set x ""
    for {set i 0} {$i < 1000} {set i [expr $i+1]} {
	append x "foobar "
    }
    set y "foobar"
    set y "$y $y $y $y $y $y $y $y $y $y"
    set y "$y $y $y $y $y $y $y $y $y $y"
    set y "$y $y $y $y $y $y $y $y $y $y "
    expr {$x eq $y}
} 1

test append-3.1 {append errors} {
    list [catch {append} msg] $msg
} {1 {wrong # args: should be "append varName ?value value ...?"}}
#test append-3.2 {append errors} {
#    set x ""
#    list [catch {append x(0) 44} msg] $msg
#} {1 {can't set "x(0)": variable isn't array}}
test append-3.3 {append errors} {
    catch {unset x}
    list [catch {append x} msg] $msg
} {1 {can't read "x": no such variable}}

test append-4.1 {lappend command} {
    catch {unset x}
    list [lappend x 1 2 abc "long string"] $x
} {{1 2 abc {long string}} {1 2 abc {long string}}}
test append-4.2 {lappend command} {
    set x ""
    list [lappend x first] [lappend x second] [lappend x third] $x
} {first {first second} {first second third} {first second third}}
test append-4.3 {lappend command} {
    proc foo {} {
	global x
	set x old
	unset x
	lappend x new
    }
    set result [foo]
    rename foo {}
    set result
} {new}
test append-4.4 {lappend command} {
    set x {}
    lappend x \{\  abc
} {\{\  abc}
test append-4.5 {lappend command} {
    set x {}
    lappend x \{ abc
} {\{ abc}
test append-4.6 {lappend command} {
    set x {1 2 3}
    lappend x
} {1 2 3}
test append-4.7 {lappend command} {
    set x "a\{"
    lappend x abc
} "a\\\{ abc"
test append-4.8 {lappend command} {
    set x "\\\{"
    lappend x abc
} "\\{ abc"
#test append-4.9 {lappend command} {
#    set x " \{"
#    list [catch {lappend x abc} msg] $msg
#} {1 {unmatched open brace in list}}
#test append-4.10 {lappend command} {
#    set x "	\{"
#    list [catch {lappend x abc} msg] $msg
#} {1 {unmatched open brace in list}}
#test append-4.11 {lappend command} {
#    set x "\{\{\{"
#    list [catch {lappend x abc} msg] $msg
#} {1 {unmatched open brace in list}}
#test append-4.12 {lappend command} {
#    set x "x \{\{\{"
#    list [catch {lappend x abc} msg] $msg
#} {1 {unmatched open brace in list}}
test append-4.13 {lappend command} {
    set x "x\{\{\{"
    lappend x abc
} "x\\\{\\\{\\\{ abc"
test append-4.14 {lappend command} {
    set x " "
    lappend x abc
} "abc"
test append-4.15 {lappend command} {
    set x "\\ "
    lappend x abc
} "{ } abc"
test append-4.16 {lappend command} {
    set x "x "
    lappend x abc
} "x abc"
test append-4.17 {lappend command} {
    catch {unset x}
    lappend x
} {}
test append-4.18 {lappend command} {
    catch {unset x}
    lappend x {}
} {{}}
test append-4.19 {lappend command} {
    catch {unset x}
    lappend x(0)
} {}
test append-4.20 {lappend command} {
    catch {unset x}
    lappend x(0) abc
} {abc}

proc check {var size} {
    set l [llength $var]
    if {$l != $size} {
	return "length mismatch: should have been $size, was $l"
    }
    for {set i 0} {$i < $size} {set i [expr $i+1]} {
	set j [lindex $var $i]
	if {$j ne "item $i"} {
	    return "element $i should have been \"item $i\", was \"$j\""
	}
    }
    return ok
}
test append-5.1 {long lappends} {
    catch {unset x}
    set x ""
    for {set i 0} {$i < 300} {set i [expr $i+1]} {
	lappend x "item $i"
    }
    check $x 300
} ok

test append-6.1 {lappend errors} {
    list [catch {lappend} msg] $msg
} {1 {wrong # args: should be "lappend varName ?value value ...?"}}
#test append-6.2 {lappend errors} {
#    set x ""
#    list [catch {lappend x(0) 44} msg] $msg
#} {1 {can't set "x(0)": variable isn't array}}

################################################################################
# UPLEVEL
################################################################################

proc a {x y} {
    newset z [expr $x+$y]
    return $z
}
proc newset {name value} {
    uplevel set $name $value
    uplevel 1 {uplevel 1 {set xyz 22}}
}

test uplevel-1.1 {simple operation} {
    set xyz 0
    a 22 33
} 55
test uplevel-1.2 {command is another uplevel command} {
    set xyz 0
    a 22 33
    set xyz
} 22

proc a1 {} {
    b1
    global a a1
    set a $x
    set a1 $y
}
proc b1 {} {
    c1
    global b b1
    set b $x
    set b1 $y
}
proc c1 {} {
    uplevel 1 set x 111
    uplevel #2 set y 222
    uplevel 2 set x 333
    uplevel #1 set y 444
    uplevel 3 set x 555
    uplevel #0 set y 666
}
a1
test uplevel-2.1 {relative and absolute uplevel} {set a} 333
test uplevel-2.2 {relative and absolute uplevel} {set a1} 444
test uplevel-2.3 {relative and absolute uplevel} {set b} 111
test uplevel-2.4 {relative and absolute uplevel} {set b1} 222
test uplevel-2.5 {relative and absolute uplevel} {set x} 555
test uplevel-2.6 {relative and absolute uplevel} {set y} 666

test uplevel-3.1 {uplevel to same level} {
    set x 33
    uplevel #0 set x 44
    set x
} 44
test uplevel-3.2 {uplevel to same level} {
    set x 33
    uplevel 0 set x
} 33
test uplevel-3.3 {uplevel to same level} {
    set y xxx
    proc a1 {} {set y 55; uplevel 0 set y 66; return $y}
    a1
} 66
test uplevel-3.4 {uplevel to same level} {
    set y zzz
    proc a1 {} {set y 55; uplevel #1 set y}
    a1
} 55

test uplevel-4.1 {error: non-existent level} {
    list [catch c1 msg] $msg
} {1 {bad level "#2"}}
test uplevel-4.2 {error: non-existent level} {
    proc c2 {} {uplevel 3 {set a b}}
    list [catch c2 msg] $msg
} {1 {bad level "3"}}
test uplevel-4.3 {error: not enough args} {
    list [catch uplevel msg] $msg
} {1 {wrong # args: should be "uplevel ?level? command ?arg ...?"}}
test uplevel-4.4 {error: not enough args} {
    proc upBug {} {uplevel 1}
    list [catch upBug msg] $msg
} {1 {wrong # args: should be "uplevel ?level? command ?arg ...?"}}

#proc a2 {} {
#    uplevel a3
#}
#proc a3 {} {
#    global x y
#    set x [info level]
#    set y [info level 1]
#}
#a2
#test uplevel-5.1 {info level} {set x} 1
#test uplevel-5.2 {info level} {set y} a3

################################################################################
# UNKNOWN
################################################################################

catch {unset x}
catch {rename unknown unknown.old}

test unknown-1.1 {non-existent "unknown" command} {
    list [catch {_non-existent_ foo bar} msg] $msg
} {1 {invalid command name "_non-existent_"}}

proc unknown {args} {
    global x
    set x $args
}

test unknown-2.1 {calling "unknown" command} {
    foobar x y z
    set x
} {foobar x y z}
test unknown-2.2 {calling "unknown" command with lots of args} {
    foobar 1 2 3 4 5 6 7
    set x
} {foobar 1 2 3 4 5 6 7}
test unknown-2.3 {calling "unknown" command with lots of args} {
    foobar 1 2 3 4 5 6 7 8
    set x
} {foobar 1 2 3 4 5 6 7 8}
test unknown-2.4 {calling "unknown" command with lots of args} {
    foobar 1 2 3 4 5 6 7 8 9
    set x
} {foobar 1 2 3 4 5 6 7 8 9}

test unknown-3.1 {argument quoting in calls to "unknown"} {
    foobar \{ \} a\{b \; "\\" \$a a\[b \]
    set x
} "foobar \\{ \\} a\\{b {;} \\\\ {\$a} {a\[b} \\]"

proc unknown args {
    error "unknown failed"
}

rename unknown {}

#test unknown-4.1 {errors in "unknown" procedure} {
#    list [catch {non-existent a b} msg] $msg $errorCode
#} {1 {unknown failed} NONE}

################################################################################
# INCR
################################################################################

catch {unset x}
catch {unset i}

test incr-1.1 {TclCompileIncrCmd: missing variable name} {
    list [catch {incr} msg] $msg
} {1 {wrong # args: should be "incr varName ?increment?"}}
test incr-1.2 {TclCompileIncrCmd: simple variable name} {
    set i 10
    list [incr i] $i
} {11 11}
#test incr-1.3 {TclCompileIncrCmd: error compiling variable name} {
#    set i 10
#    catch {incr "i"xxx} msg
#    set msg
#} {extra characters after close-quote}
test incr-1.4 {TclCompileIncrCmd: simple variable name in quotes} {
    set i 17
    list [incr "i"] $i
} {18 18}
test incr-1.5 {TclCompileIncrCmd: simple variable name in braces} {
    catch {unset {a simple var}}
    set {a simple var} 27
    list [incr {a simple var}] ${a simple var}
} {28 28}
test incr-1.6 {TclCompileIncrCmd: simple array variable name} {
    catch {unset a}
    set a(foo) 37
    list [incr a(foo)] $a(foo)
} {38 38}
test incr-1.7 {TclCompileIncrCmd: non-simple (computed) variable name} {
    set x "i"
    set i 77
    list [incr $x 2] $i
} {79 79}
test incr-1.8 {TclCompileIncrCmd: non-simple (computed) variable name} {
    set x "i"
    set i 77
    list [incr [set x] +2] $i
} {79 79}

test incr-1.9 {TclCompileIncrCmd: increment given} {
    set i 10
    list [incr i +07] $i
} {17 17}
test incr-1.10 {TclCompileIncrCmd: no increment given} {
    set i 10
    list [incr i] $i
} {11 11}

test incr-1.11 {TclCompileIncrCmd: simple global name} {
    proc p {} {
        global i
        set i 54
        incr i
    }
    p
} {55}
test incr-1.12 {TclCompileIncrCmd: simple local name} {
    proc p {} {
        set foo 100
        incr foo
    }
    p
} {101}
test incr-1.13 {TclCompileIncrCmd: simple but new (unknown) local name} {
    proc p {} {
        incr bar
    }
    catch {p} msg
    set msg
} {can't read "bar": no such variable}
test incr-1.14 {TclCompileIncrCmd: simple local name, >255 locals} {
    proc 260locals {} {
        # create 260 locals
        set a0 0; set a1 0; set a2 0; set a3 0; set a4 0
        set a5 0; set a6 0; set a7 0; set a8 0; set a9 0
        set b0 0; set b1 0; set b2 0; set b3 0; set b4 0
        set b5 0; set b6 0; set b7 0; set b8 0; set b9 0
        set c0 0; set c1 0; set c2 0; set c3 0; set c4 0
        set c5 0; set c6 0; set c7 0; set c8 0; set c9 0
        set d0 0; set d1 0; set d2 0; set d3 0; set d4 0
        set d5 0; set d6 0; set d7 0; set d8 0; set d9 0
        set e0 0; set e1 0; set e2 0; set e3 0; set e4 0
        set e5 0; set e6 0; set e7 0; set e8 0; set e9 0
        set f0 0; set f1 0; set f2 0; set f3 0; set f4 0
        set f5 0; set f6 0; set f7 0; set f8 0; set f9 0
        set g0 0; set g1 0; set g2 0; set g3 0; set g4 0
        set g5 0; set g6 0; set g7 0; set g8 0; set g9 0
        set h0 0; set h1 0; set h2 0; set h3 0; set h4 0
        set h5 0; set h6 0; set h7 0; set h8 0; set h9 0
        set i0 0; set i1 0; set i2 0; set i3 0; set i4 0
        set i5 0; set i6 0; set i7 0; set i8 0; set i9 0
        set j0 0; set j1 0; set j2 0; set j3 0; set j4 0
        set j5 0; set j6 0; set j7 0; set j8 0; set j9 0
        set k0 0; set k1 0; set k2 0; set k3 0; set k4 0
        set k5 0; set k6 0; set k7 0; set k8 0; set k9 0
        set l0 0; set l1 0; set l2 0; set l3 0; set l4 0
        set l5 0; set l6 0; set l7 0; set l8 0; set l9 0
        set m0 0; set m1 0; set m2 0; set m3 0; set m4 0
        set m5 0; set m6 0; set m7 0; set m8 0; set m9 0
        set n0 0; set n1 0; set n2 0; set n3 0; set n4 0
        set n5 0; set n6 0; set n7 0; set n8 0; set n9 0
        set o0 0; set o1 0; set o2 0; set o3 0; set o4 0
        set o5 0; set o6 0; set o7 0; set o8 0; set o9 0
        set p0 0; set p1 0; set p2 0; set p3 0; set p4 0
        set p5 0; set p6 0; set p7 0; set p8 0; set p9 0
        set q0 0; set q1 0; set q2 0; set q3 0; set q4 0
        set q5 0; set q6 0; set q7 0; set q8 0; set q9 0
        set r0 0; set r1 0; set r2 0; set r3 0; set r4 0
        set r5 0; set r6 0; set r7 0; set r8 0; set r9 0
        set s0 0; set s1 0; set s2 0; set s3 0; set s4 0
        set s5 0; set s6 0; set s7 0; set s8 0; set s9 0
        set t0 0; set t1 0; set t2 0; set t3 0; set t4 0
        set t5 0; set t6 0; set t7 0; set t8 0; set t9 0
        set u0 0; set u1 0; set u2 0; set u3 0; set u4 0
        set u5 0; set u6 0; set u7 0; set u8 0; set u9 0
        set v0 0; set v1 0; set v2 0; set v3 0; set v4 0
        set v5 0; set v6 0; set v7 0; set v8 0; set v9 0
        set w0 0; set w1 0; set w2 0; set w3 0; set w4 0
        set w5 0; set w6 0; set w7 0; set w8 0; set w9 0
        set x0 0; set x1 0; set x2 0; set x3 0; set x4 0
        set x5 0; set x6 0; set x7 0; set x8 0; set x9 0
        set y0 0; set y1 0; set y2 0; set y3 0; set y4 0
        set y5 0; set y6 0; set y7 0; set y8 0; set y9 0
        set z0 0; set z1 0; set z2 0; set z3 0; set z4 0
        set z5 0; set z6 0; set z7 0; set z8 0; set z9 0
        # now increment the last one (local var index > 255)
        incr z9
    }
    260locals
} {1}
test incr-1.15 {TclCompileIncrCmd: variable is array} {
    catch {unset a}
    set a(foo) 27
    set x [incr a(foo) 11]
    catch {unset a}
    set x
} 38
test incr-1.16 {TclCompileIncrCmd: variable is array, elem substitutions} {
    catch {unset a}
    set i 5
    set a(foo5) 27
    set x [incr a(foo$i) 11]
    catch {unset a}
    set x
} 38

test incr-1.17 {TclCompileIncrCmd: increment given, simple int} {
    set i 5
    incr i 123
} 128
test incr-1.18 {TclCompileIncrCmd: increment given, simple int} {
    set i 5
    incr i -100
} -95
#test incr-1.19 {TclCompileIncrCmd: increment given, but erroneous} {
#    set i 5
#    catch {incr i [set]} msg
#    set errorInfo
#} {wrong # args: should be "set varName ?newValue?"
#    while compiling
#"set"
#    while compiling
#"incr i [set]"}
test incr-1.20 {TclCompileIncrCmd: increment given, in quotes} {
    set i 25
    incr i "-100"
} -75
test incr-1.21 {TclCompileIncrCmd: increment given, in braces} {
    set i 24
    incr i {126}
} 150
test incr-1.22 {TclCompileIncrCmd: increment given, large int} {
    set i 5
    incr i 200000
} 200005
test incr-1.23 {TclCompileIncrCmd: increment given, formatted int != int} {
    set i 25
    incr i 000012345     ;# an octal literal
} 5374
test incr-1.24 {TclCompileIncrCmd: increment given, formatted int != int} {
    set i 25
    catch {incr i 1a} msg
    set msg
} {expected integer but got "1a"}

test incr-1.25 {TclCompileIncrCmd: too many arguments} {
    set i 10
    catch {incr i 10 20} msg
    set msg
} {wrong # args: should be "incr varName ?increment?"}


test incr-1.29 {TclCompileIncrCmd: runtime error, bad variable value} {
    set x "  -  "
    list [catch {incr x 1} msg] $msg
} {1 {expected integer but got "  -  "}}

test incr-1.30 {TclCompileIncrCmd: array var, braced (no subs)} {
    catch {unset array}
    set array(\$foo) 4
    incr {array($foo)}
} 5
    
# Check "incr" and computed command names.

test incr-2.0 {incr and computed command names} {
    set i 5
    set z incr
    $z i -1
    set i
} 4
catch {unset x}
catch {unset i}

test incr-2.1 {incr command (not compiled): missing variable name} {
    set z incr
    list [catch {$z} msg] $msg
} {1 {wrong # args: should be "incr varName ?increment?"}}
test incr-2.2 {incr command (not compiled): simple variable name} {
    set z incr
    set i 10
    list [$z i] $i
} {11 11}
test incr-2.4 {incr command (not compiled): simple variable name in quotes} {
    set z incr
    set i 17
    list [$z "i"] $i
} {18 18}
test incr-2.5 {incr command (not compiled): simple variable name in braces} {
    set z incr
    catch {unset {a simple var}}
    set {a simple var} 27
    list [$z {a simple var}] ${a simple var}
} {28 28}
test incr-2.6 {incr command (not compiled): simple array variable name} {
    set z incr
    catch {unset a}
    set a(foo) 37
    list [$z a(foo)] $a(foo)
} {38 38}
test incr-2.7 {incr command (not compiled): non-simple (computed) variable name} {
    set z incr
    set x "i"
    set i 77
    list [$z $x 2] $i
} {79 79}
test incr-2.8 {incr command (not compiled): non-simple (computed) variable name} {
    set z incr
    set x "i"
    set i 77
    list [$z [set x] +2] $i
} {79 79}

test incr-2.9 {incr command (not compiled): increment given} {
    set z incr
    set i 10
    list [$z i +07] $i
} {17 17}
test incr-2.10 {incr command (not compiled): no increment given} {
    set z incr
    set i 10
    list [$z i] $i
} {11 11}

test incr-2.11 {incr command (not compiled): simple global name} {
    proc p {} {
	set z incr
        global i
        set i 54
        $z i
    }
    p
} {55}
test incr-2.12 {incr command (not compiled): simple local name} {
    proc p {} {
	set z incr
        set foo 100
        $z foo
    }
    p
} {101}
test incr-2.13 {incr command (not compiled): simple but new (unknown) local name} {
    proc p {} {
	set z incr
        $z bar
    }
    catch {p} msg
    set msg
} {can't read "bar": no such variable}
test incr-2.14 {incr command (not compiled): simple local name, >255 locals} {
   proc 260locals {} {
        set z incr
        # create 260 locals
        set a0 0; set a1 0; set a2 0; set a3 0; set a4 0
        set a5 0; set a6 0; set a7 0; set a8 0; set a9 0
        set b0 0; set b1 0; set b2 0; set b3 0; set b4 0
        set b5 0; set b6 0; set b7 0; set b8 0; set b9 0
        set c0 0; set c1 0; set c2 0; set c3 0; set c4 0
        set c5 0; set c6 0; set c7 0; set c8 0; set c9 0
        set d0 0; set d1 0; set d2 0; set d3 0; set d4 0
        set d5 0; set d6 0; set d7 0; set d8 0; set d9 0
        set e0 0; set e1 0; set e2 0; set e3 0; set e4 0
        set e5 0; set e6 0; set e7 0; set e8 0; set e9 0
        set f0 0; set f1 0; set f2 0; set f3 0; set f4 0
        set f5 0; set f6 0; set f7 0; set f8 0; set f9 0
        set g0 0; set g1 0; set g2 0; set g3 0; set g4 0
        set g5 0; set g6 0; set g7 0; set g8 0; set g9 0
        set h0 0; set h1 0; set h2 0; set h3 0; set h4 0
        set h5 0; set h6 0; set h7 0; set h8 0; set h9 0
        set i0 0; set i1 0; set i2 0; set i3 0; set i4 0
        set i5 0; set i6 0; set i7 0; set i8 0; set i9 0
        set j0 0; set j1 0; set j2 0; set j3 0; set j4 0
        set j5 0; set j6 0; set j7 0; set j8 0; set j9 0
        set k0 0; set k1 0; set k2 0; set k3 0; set k4 0
        set k5 0; set k6 0; set k7 0; set k8 0; set k9 0
        set l0 0; set l1 0; set l2 0; set l3 0; set l4 0
        set l5 0; set l6 0; set l7 0; set l8 0; set l9 0
        set m0 0; set m1 0; set m2 0; set m3 0; set m4 0
        set m5 0; set m6 0; set m7 0; set m8 0; set m9 0
        set n0 0; set n1 0; set n2 0; set n3 0; set n4 0
        set n5 0; set n6 0; set n7 0; set n8 0; set n9 0
        set o0 0; set o1 0; set o2 0; set o3 0; set o4 0
        set o5 0; set o6 0; set o7 0; set o8 0; set o9 0
        set p0 0; set p1 0; set p2 0; set p3 0; set p4 0
        set p5 0; set p6 0; set p7 0; set p8 0; set p9 0
        set q0 0; set q1 0; set q2 0; set q3 0; set q4 0
        set q5 0; set q6 0; set q7 0; set q8 0; set q9 0
        set r0 0; set r1 0; set r2 0; set r3 0; set r4 0
        set r5 0; set r6 0; set r7 0; set r8 0; set r9 0
        set s0 0; set s1 0; set s2 0; set s3 0; set s4 0
        set s5 0; set s6 0; set s7 0; set s8 0; set s9 0
        set t0 0; set t1 0; set t2 0; set t3 0; set t4 0
        set t5 0; set t6 0; set t7 0; set t8 0; set t9 0
        set u0 0; set u1 0; set u2 0; set u3 0; set u4 0
        set u5 0; set u6 0; set u7 0; set u8 0; set u9 0
        set v0 0; set v1 0; set v2 0; set v3 0; set v4 0
        set v5 0; set v6 0; set v7 0; set v8 0; set v9 0
        set w0 0; set w1 0; set w2 0; set w3 0; set w4 0
        set w5 0; set w6 0; set w7 0; set w8 0; set w9 0
        set x0 0; set x1 0; set x2 0; set x3 0; set x4 0
        set x5 0; set x6 0; set x7 0; set x8 0; set x9 0
        set y0 0; set y1 0; set y2 0; set y3 0; set y4 0
        set y5 0; set y6 0; set y7 0; set y8 0; set y9 0
        set z0 0; set z1 0; set z2 0; set z3 0; set z4 0
        set z5 0; set z6 0; set z7 0; set z8 0; set z9 0
        # now increment the last one (local var index > 255)
        $z z9
    }
    260locals
} {1}
test incr-2.15 {incr command (not compiled): variable is array} {
    set z incr
    catch {unset a}
    set a(foo) 27
    set x [$z a(foo) 11]
    catch {unset a}
    set x
} 38
test incr-2.16 {incr command (not compiled): variable is array, elem substitutions} {
    set z incr
    catch {unset a}
    set i 5
    set a(foo5) 27
    set x [$z a(foo$i) 11]
    catch {unset a}
    set x
} 38

test incr-2.17 {incr command (not compiled): increment given, simple int} {
    set z incr
    set i 5
    $z i 123
} 128
test incr-2.18 {incr command (not compiled): increment given, simple int} {
    set z incr
    set i 5
    $z i -100
} -95
test incr-2.20 {incr command (not compiled): increment given, in quotes} {
    set z incr
    set i 25
    $z i "-100"
} -75
test incr-2.21 {incr command (not compiled): increment given, in braces} {
    set z incr
    set i 24
    $z i {126}
} 150
test incr-2.22 {incr command (not compiled): increment given, large int} {
    set z incr
    set i 5
    $z i 200000
} 200005
test incr-2.23 {incr command (not compiled): increment given, formatted int != int} {
    set z incr
    set i 25
    $z i 000012345     ;# an octal literal
} 5374
test incr-2.24 {incr command (not compiled): increment given, formatted int != int} {
    set z incr
    set i 25
    catch {$z i 1a} msg
    set msg
} {expected integer but got "1a"}

test incr-2.25 {incr command (not compiled): too many arguments} {
    set z incr
    set i 10
    catch {$z i 10 20} msg
    set msg
} {wrong # args: should be "incr varName ?increment?"}

test incr-2.29 {incr command (not compiled): runtime error, bad variable value} {
    set z incr
    set x "  -  "
    list [catch {$z x 1} msg] $msg
} {1 {expected integer but got "  -  "}}

################################################################################
# LLENGTH
################################################################################

test llength-1.1 {length of list} {
    llength {a b c d}
} 4
test llength-1.2 {length of list} {
    llength {a b c {a b {c d}} d}
} 5
test llength-1.3 {length of list} {
    llength {}
} 0

test llength-2.1 {error conditions} {
    list [catch {llength} msg] $msg
} {1 {wrong # args: should be "llength list"}}
test llength-2.2 {error conditions} {
    list [catch {llength 123 2} msg] $msg
} {1 {wrong # args: should be "llength list"}}

################################################################################
# LINDEX
################################################################################

set lindex lindex
set minus -

# Tests of Tcl_LindexObjCmd, NOT COMPILED

#test lindex-1.1 {wrong # args} {
#    list [catch {eval $lindex} result] $result
#} "1 {wrong # args: should be \"lindex list ?index...?\"}"

# Indices that are lists or convertible to lists

#test lindex-2.1 {empty index list} {
#    set x {}
#    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
#} {{a b c} {a b c}}

test lindex-2.2 {singleton index list} {
    set x { 1 }
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {b b}

test lindex-2.4 {malformed index list} {
    set x \{
    list [catch { eval [list $lindex {a b c} $x] } result] $result
} {1 bad\ index\ \"\{\":\ must\ be\ integer\ or\ end?-integer?}

# Indices that are integers or convertible to integers

test lindex-3.1 {integer -1} {
    set x ${minus}1
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {{} {}}

test lindex-3.2 {integer 0} {
    set x [string range 00 0 0]
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {a a}

test lindex-3.3 {integer 2} {
    set x [string range 22 0 0]
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {c c}

test lindex-3.4 {integer 3} {
    set x [string range 33 0 0]
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {{} {}}

test lindex-3.7 {indexes don't shimmer wide ints} {
    set x [expr {(1<<31) - 2}]
    list $x [lindex {1 2 3} $x] [incr x] [incr x]
} {2147483646 {} 2147483647 2147483648}

# Indices relative to end

test lindex-4.1 {index = end} {
    set x end
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {c c}

test lindex-4.2 {index = end--1} {
    set x end--1
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {{} {}}

test lindex-4.3 {index = end-0} {
    set x end-0
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {c c}

test lindex-4.4 {index = end-2} {
    set x end-2
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {a a}

test lindex-4.5 {index = end-3} {
    set x end-3
    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
} {{} {}}

test lindex-4.8 {bad integer, not octal} {
    set x end-0a2
    list [catch { eval [list $lindex {a b c} $x] } result] $result
} "1 {bad index \"end-0a2\": must be integer or end?-integer?}"

#test lindex-4.9 {incomplete end} {
#    set x en
#    list [eval [list $lindex {a b c} $x]] [eval [list $lindex {a b c} $x]]
#} {c c}

test lindex-4.10 {incomplete end-} {
    set x end-
    list [catch { eval [list $lindex {a b c} $x] } result] $result
} "1 {bad index \"end-\": must be integer or end?-integer?}"

test lindex-5.1 {bad second index} {
    list [catch { eval [list $lindex {a b c} 0 0a2] } result] $result
} "1 {bad index \"0a2\": must be integer or end?-integer?}"

test lindex-5.2 {good second index} {
    eval [list $lindex {{a b c} {d e f} {g h i}} 1 2]
} f

test lindex-5.3 {three indices} {
    eval [list $lindex {{{a b} {c d}} {{e f} {g h}}} 1 0 1]
} f

test lindex-7.1 {quoted elements} {
    eval [list $lindex {a "b c" d} 1]
} {b c}
test lindex-7.2 {quoted elements} {
    eval [list $lindex {"{}" b c} 0]
} {{}}
test lindex-7.3 {quoted elements} {
    eval [list $lindex {ab "c d \" x" y} 1]
} {c d " x}
test lindex-7.4 {quoted elements} {
    lindex {a b {c d "e} {f g"}} 2
} {c d "e}

test lindex-8.1 {data reuse} {
    set x 0
    eval [list $lindex $x $x]
} {0}

test lindex-8.2 {data reuse} {
    set a 0
    eval [list $lindex $a $a $a]
} 0
test lindex-8.3 {data reuse} {
    set a 1
    eval [list $lindex $a $a $a]
} {}

#----------------------------------------------------------------------

test lindex-10.2 {singleton index list} {
    set x { 1 }
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {b b}

test lindex-10.4 {malformed index list} {
    set x \{
    list [catch { lindex {a b c} $x } result] $result
} {1 bad\ index\ \"\{\":\ must\ be\ integer\ or\ end?-integer?}

# Indices that are integers or convertible to integers

test lindex-11.1 {integer -1} {
    set x ${minus}1
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {{} {}}

test lindex-11.2 {integer 0} {
    set x [string range 00 0 0]
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {a a}

test lindex-11.3 {integer 2} {
    set x [string range 22 0 0]
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {c c}

test lindex-11.4 {integer 3} {
    set x [string range 33 0 0]
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {{} {}}

# Indices relative to end
test lindex-12.1 {index = end} {
    set x end
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {c c}

test lindex-12.2 {index = end--1} {
    set x end--1
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {{} {}}

test lindex-12.3 {index = end-0} {
    set x end-0
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {c c}

test lindex-12.4 {index = end-2} {
    set x end-2
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {a a}

test lindex-12.5 {index = end-3} {
    set x end-3
    catch {
	list [lindex {a b c} $x] [lindex {a b c} $x]
    } result
    set result
} {{} {}}

test lindex-12.8 {bad integer, not octal} {
    set x end-0a2
    list [catch { lindex {a b c} $x } result] $result
} "1 {bad index \"end-0a2\": must be integer or end?-integer?}"

test lindex-12.10 {incomplete end-} {
    set x end-
    list [catch { lindex {a b c} $x } result] $result
} "1 {bad index \"end-\": must be integer or end?-integer?}"

test lindex-13.1 {bad second index} {
    list [catch { lindex {a b c} 0 0a2 } result] $result
} "1 {bad index \"0a2\": must be integer or end?-integer?}"

test lindex-13.2 {good second index} {
    catch {
	lindex {{a b c} {d e f} {g h i}} 1 2
    } result
    set result
} f

test lindex-13.3 {three indices} {
    catch {
	lindex {{{a b} {c d}} {{e f} {g h}}} 1 0 1
    } result
    set result
} f

test lindex-15.1 {quoted elements} {
    catch {
	lindex {a "b c" d} 1
    } result
    set result
} {b c}
test lindex-15.2 {quoted elements} {
    catch {
	lindex {"{}" b c} 0
    } result
    set result
} {{}}
test lindex-15.3 {quoted elements} {
    catch {
	lindex {ab "c d \" x" y} 1
    } result
    set result
} {c d " x}
test lindex-15.4 {quoted elements} {
    catch {
	lindex {a b {c d "e} {f g"}} 2
    } result
    set result
} {c d "e}

test lindex-16.1 {data reuse} {
    set x 0
    catch {
	lindex $x $x
    } result
    set result
} {0}

test lindex-16.2 {data reuse} {
    set a 0
    catch {
	lindex $a $a $a
    } result
    set result
} 0
test lindex-16.3 {data reuse} {
    set a 1
    catch {
	lindex $a $a $a
    } result
    set result
} {}

catch { unset lindex}
catch { unset minus }

################################################################################
# LINDEX
################################################################################

catch {unset a}
catch {unset x}

# Basic "foreach" operation.

test foreach-1.1 {basic foreach tests} {
	set a {}
	foreach i {a b c d} {
		set a [concat $a $i]
	}
	set a
} {a b c d}
test foreach-1.2 {basic foreach tests} {
  set a {}
  foreach i {a b {{c d} e} {123 {{x}}}} {
		set a [concat $a $i]
	}
  set a
} {a b {c d} e 123 {{x}}}
test foreach-1.3 {basic foreach tests} {catch {foreach} msg} 1
test foreach-1.4 {basic foreach tests} {catch {foreach i} msg} 1
test foreach-1.5 {basic foreach tests} {catch {foreach i j} msg} 1
test foreach-1.6 {basic foreach tests} {catch {foreach i j k l} msg} 1
test foreach-1.7 {basic foreach tests} {
  set a {}
  foreach i {} {
		set a [concat $a $i]
	}
  set a
} {}
catch {unset a}
test foreach-2.1 {foreach errors} {
    list [catch {foreach {} {} {}} msg] $msg
} {1 {foreach varlist is empty}}
catch {unset a}

test foreach-3.1 {parallel foreach tests} {
  set x {}
  foreach {a b} {1 2 3 4} {
		append x $b $a
	}
  set x
} {2143}
test foreach-3.2 {parallel foreach tests} {
  set x {}
  foreach {a b} {1 2 3 4 5} {
		append x $b $a
  }
	set x
} {21435}
test foreach-3.3 {parallel foreach tests} {
  set x {}
  foreach a {1 2 3} b {4 5 6} {
		append x $b $a
	}
  set x
} {415263}
test foreach-3.4 {parallel foreach tests} {
  set x {}
  foreach a {1 2 3} b {4 5 6 7 8} {
		append x $b $a
	}
  set x
} {41526378}
test foreach-3.5 {parallel foreach tests} {
  set x {}
  foreach {a b} {a b A B aa bb} c {c C cc CC} {
		append x $a $b $c
	}
  set x
} {abcABCaabbccCC}
test foreach-3.6 {parallel foreach tests} {
  set x {}
  foreach a {1 2 3} b {1 2 3} c {1 2 3} d {1 2 3} e {1 2 3} {
		append x $a $b $c $d $e
  }
	set x
} {111112222233333}
test foreach-3.7 {parallel foreach tests} {
  set x {}
  foreach a {} b {1 2 3} c {1 2} d {1 2 3 4} e {{1 2}} {
		append x $a $b $c $d $e
  }
	set x
} {1111 2222334}
test foreach-4.1 {foreach only sets vars if repeating loop} {
  proc foo {} {
		set rgb {65535 0 0}
		foreach {r g b} [set rgb] {}
		return "r=$r, g=$g, b=$b"
	}
	foo
} {r=65535, g=0, b=0}
test foreach-5.1 {foreach supports dict syntactic sugar} {
	proc foo {} {
    set x {}
    foreach {a(3)} {1 2 3 4} {lappend x [set {a(3)}]}
		list $a $x
	}
	foo
} {{3 4} {1 2 3 4}}

test foreach-6.1 {noncompiled foreach and shared variable or value list objects that are converted to another type} {
  catch {unset x}
  foreach {12.0} {a b c} {
    set x 12.0  
    set x [expr $x + 1]
  }
  set x
} 13.0

# Check "continue".

test foreach-7.1 {continue tests} {catch continue} 4
test foreach-7.2 {continue tests} {
  set a {}
  foreach i {a b c d} {
		if {[string compare $i "b"] == 0} continue
		set a [concat $a $i]
	}
   set a
} {a c d}
test foreach-7.3 {continue tests} {
	set a {}
  foreach i {a b c d} {
		if {[string compare $i "b"] != 0} continue
		set a [concat $a $i]
	}
  set a
} {b}
test foreach-7.4 {continue tests} {catch {continue foo} msg} 1
test foreach-7.5 {continue tests} {
	catch {continue foo} msg
  set msg
} {wrong # args: should be "continue"}

# Check "break".

test foreach-8.1 {break tests} {catch break} 3
test foreach-8.2 {break tests} {
  set a {}
	foreach i {a b c d} {
		if {[string compare $i "c"] == 0} break
		set a [concat $a $i]
	}
  set a
} {a b}
test foreach-8.3 {break tests} {catch {break foo} msg} 1
test foreach-8.4 {break tests} {
  catch {break foo} msg
  set msg
} {wrong # args: should be "break"}

# Test for incorrect "double evaluation" semantics

test foreach-9.1 {delayed substitution of body - knownbugs} {
  proc foo {} {
    set a 0
    foreach a [list 1 2 3] "
      set x $a
    "
    set x
  }
  foo
} {0}

# cleanup
catch {unset a}
catch {unset x}

################################################################################
# STRING
################################################################################

## string match
##
test string-11.1 {string match, too few args} {
    proc foo {} {string match a}
    list [catch {foo} msg] $msg
} {1 {wrong # args: should be "string match ?-nocase? pattern string"}}
test string-11.2 {string match, too many args} {
    proc foo {} {string match a b c d}
    list [catch {foo} msg] $msg
} {1 {wrong # args: should be "string match ?-nocase? pattern string"}}
test string-11.3 {string match} {
    proc foo {} {string match abc abc}
    foo
} 1
#test string-11.4 {string match} {
#    proc foo {} {string mat abc abd}
#    foo
#} 0
test string-11.5 {string match} {
    proc foo {} {string match ab*c abc}
    foo
} 1
test string-11.6 {string match} {
    proc foo {} {string match ab**c abc}
    foo
} 1
test string-11.7 {string match} {
    proc foo {} {string match ab* abcdef}
    foo
} 1
test string-11.8 {string match} {
    proc foo {} {string match *c abc}
    foo
} 1
test string-11.9 {string match} {
    proc foo {} {string match *3*6*9 0123456789}
    foo
} 1
test string-11.10 {string match} {
    proc foo {} {string match *3*6*9 01234567890}
    foo
} 0
test string-11.11 {string match} {
    proc foo {} {string match a?c abc}
    foo
} 1
test string-11.12 {string match} {
    proc foo {} {string match a??c abc}
    foo
} 0
test string-11.13 {string match} {
    proc foo {} {string match ?1??4???8? 0123456789}
    foo
} 1
test string-11.14 {string match} {
    proc foo {} {string match {[abc]bc} abc}
    foo
} 1
test string-11.15 {string match} {
    proc foo {} {string match {a[abc]c} abc}
    foo
} 1
test string-11.16 {string match} {
    proc foo {} {string match {a[xyz]c} abc}
    foo
} 0
test string-11.17 {string match} {
    proc foo {} {string match {12[2-7]45} 12345}
    foo
} 1
test string-11.18 {string match} {
    proc foo {} {string match {12[ab2-4cd]45} 12345}
    foo
} 1
test string-11.19 {string match} {
    proc foo {} {string match {12[ab2-4cd]45} 12b45}
    foo
} 1
test string-11.20 {string match} {
    proc foo {} {string match {12[ab2-4cd]45} 12d45}
    foo
} 1
test string-11.21 {string match} {
    proc foo {} {string match {12[ab2-4cd]45} 12145}
    foo
} 0
test string-11.22 {string match} {
    proc foo {} {string match {12[ab2-4cd]45} 12545}
    foo
} 0
test string-11.23 {string match} {
    proc foo {} {string match {a\*b} a*b}
    foo
} 1
test string-11.24 {string match} {
    proc foo {} {string match {a\*b} ab}
    foo
} 0
test string-11.25 {string match} {
    proc foo {} {string match {a\*\?\[\]\\\x} "a*?\[\]\\x"}
    foo
} 1
test string-11.26 {string match} {
    proc foo {} {string match ** ""}
    foo
} 1
test string-11.27 {string match} {
    proc foo {} {string match *. ""}
    foo
} 0
test string-11.28 {string match} {
    proc foo {} {string match "" ""}
    foo
} 1
test string-11.29 {string match} {
    proc foo {} {string match \[a a}
    foo
} 1
test string-11.31 {string match case} {
    proc foo {} {string match a A}
    foo
} 0
#test string-11.32 {string match nocase} {
#    proc foo {} {string match -n a A}
#    foo
#} 1
#test string-11.33 {string match nocase} {
#    proc foo {} {string match -nocase a\334 A\374}
#    foo
#} 1
test string-11.34 {string match nocase} {
    proc foo {} {string match -nocase a*f ABCDEf}
    foo
} 1
test string-11.35 {string match case, false hope} {
    # This is true because '_' lies between the A-Z and a-z ranges
    proc foo {} {string match {[A-z]} _}
    foo
} 1
test string-11.36 {string match nocase range} {
    # This is false because although '_' lies between the A-Z and a-z ranges,
    # we lower case the end points before checking the ranges.
    proc foo {} {string match -nocase {[A-z]} _}
    foo
} 0
test string-11.37 {string match nocase} {
    proc foo {} {string match -nocase {[A-fh-Z]} g}
    foo
} 0
test string-11.38 {string match case, reverse range} {
    proc foo {} {string match {[A-fh-Z]} g}
    foo
} 1
test string-11.39 {string match, *\ case} {
    proc foo {} {string match {*\abc} abc}
    foo
} 1
test string-11.40 {string match, *special case} {
    proc foo {} {string match {*[ab]} abc}
    foo
} 0
test string-11.41 {string match, *special case} {
    proc foo {} {string match {*[ab]*} abc}
    foo
} 1
#test string-11.42 {string match, *special case} {
#    proc foo {} {string match "*\\" "\\"}
#    foo
#} 0
test string-11.43 {string match, *special case} {
    proc foo {} {string match "*\\\\" "\\"}
    foo
} 1
test string-11.44 {string match, *special case} {
    proc foo {} {string match "*???" "12345"}
    foo
} 1
test string-11.45 {string match, *special case} {
    proc foo {} {string match "*???" "12"}
    foo
} 0
test string-11.46 {string match, *special case} {
    proc foo {} {string match "*\\*" "abc*"}
    foo
} 1
test string-11.47 {string match, *special case} {
    proc foo {} {string match "*\\*" "*"}
    foo
} 1
test string-11.48 {string match, *special case} {
    proc foo {} {string match "*\\*" "*abc"}
    foo
} 0
test string-11.49 {string match, *special case} {
    proc foo {} {string match "?\\*" "a*"}
    foo
} 1
#test string-11.50 {string match, *special case} {
#    proc foo {} {string match "\\" "\\"}
#    foo
#} 0

## string length
##
test string-9.1 {string length} {
    proc foo {} {string length}
    list [catch {foo} msg] $msg
} {1 {wrong # args: should be "string length string"}}
test string-9.2 {string length} {
    proc foo {} {string length a b}
    list [catch {foo} msg] $msg
} {1 {wrong # args: should be "string length string"}}
test string-9.3 {string length} {
    proc foo {} {string length "a little string"}
    foo
} 15

# string map

test string-10.4 {string map} {
    string map {a b} abba
} {bbbb}
test string-10.5 {string map} {
    string map {a b} a
} {b}
test string-10.6 {string map -nocase} {
    string map -nocase {a b} Abba
} {bbbb}
test string-10.7 {string map} {
    string map {abc 321 ab * a A} aabcabaababcab
} {A321*A*321*}
test string-10.8 {string map -nocase} {
    string map -nocase {aBc 321 Ab * a A} aabcabaababcab
} {A321*A*321*}
test string-10.10 {string map} {
    list [catch {string map {a b c} abba} msg] $msg
} {1 {list must contain an even number of elements}}
test string-10.11 {string map, nulls} {
    string map {\x00 NULL blah \x00nix} {qwerty}
} {qwerty}
test string-10.12 {string map, unicode} {
    string map [list \374 ue UE \334] "a\374ueUE\000EU"
} aueue\334\0EU
test string-10.13 {string map, -nocase unicode} {
    string map -nocase [list \374 ue UE \334] "a\374ueUE\000EU"
} aue\334\334\0EU
test string-10.14 {string map, -nocase null arguments} {
    string map -nocase {{} abc} foo
} foo
test string-10.15 {string map, one pair case} {
    string map -nocase {abc 32} aAbCaBaAbAbcAb
} {a32aBaAb32Ab}
test string-10.16 {string map, one pair case} {
    string map -nocase {ab 4321} aAbCaBaAbAbcAb
} {a4321C4321a43214321c4321}
test string-10.17 {string map, one pair case} {
    string map {Ab 4321} aAbCaBaAbAbcAb
} {a4321CaBa43214321c4321}
test string-10.18 {string map, empty argument} {
    string map -nocase {{} abc} foo
} foo
test string-10.19 {string map, empty arguments} {
    string map -nocase {{} abc f bar {} def} foo
} baroo

################################################################################
# SPLIT
################################################################################

test split-1.1 {basic split commands} {
    split "a\n b\t\r c\n "
} {a {} b {} {} c {} {}}
test split-1.2 {basic split commands} {
    split "word 1xyzword 2zword 3" xyz
} {{word 1} {} {} {word 2} {word 3}}
test split-1.3 {basic split commands} {
    split "12345" {}
} {1 2 3 4 5}
test split-1.4 {basic split commands} {
    split "a\}b\[c\{\]\$"
} "a\\}b\\\[c\\{\\\]\\\$"
test split-1.5 {basic split commands} {
    split {} {}
} {}
test split-1.6 {basic split commands} {
    split {}
} {}
test split-1.7 {basic split commands} {
    split {   }
} {{} {} {} {}}
test split-1.8 {basic split commands} {
    proc foo {} {
        set x {}
        foreach f [split {]\n} {}] {
            append x $f
        }
        return $x	
    }
    foo
} {]\n}
test split-1.9 {basic split commands} {
    proc foo {} {
        set x ab\000c
        set y [split $x {}]
        return $y
    }
    foo
} "a b \000 c"
test split-1.10 {basic split commands} {
    split "a0ab1b2bbb3\000c4" ab\000c
} {{} 0 {} 1 2 {} {} 3 {} 4}
test split-1.11 {basic split commands} {
    split "12,3,45" {,}
} {12 3 45}
#test split-1.12 {basic split commands} {
#    split "\u0001ab\u0001cd\u0001\u0001ef\u0001" \1
#} {{} ab cd {} ef {}}
test split-1.13 {basic split commands} {
    split "12,34,56," {,}
} {12 34 56 {}}
test split-1.14 {basic split commands} {
    split ",12,,,34,56," {,}
} {{} 12 {} {} 34 56 {}}

test split-2.1 {split errors} {
    list [catch split msg] $msg
} {1 {wrong # args: should be "split string ?splitChars?"}}
test split-2.2 {split errors} {
    list [catch {split a b c} msg] $msg
} {1 {wrong # args: should be "split string ?splitChars?"}}

# cleanup
catch {rename foo {}}

################################################################################
# JOIN
################################################################################

test join-1.1 {basic join commands} {
    join {a b c} xyz
} axyzbxyzc
test join-1.2 {basic join commands} {
    join {a b c} {}
} abc
test join-1.3 {basic join commands} {
    join {} xyz
} {}
test join-1.4 {basic join commands} {
    join {12 34 56}
} {12 34 56}

test join-2.1 {join errors} {
    list [catch join msg] $msg
} {1 {wrong # args: should be "join list ?joinString?"}}
test join-2.2 {join errors} {
    list [catch {join a b c} msg] $msg
} {1 {wrong # args: should be "join list ?joinString?"}}
#test join-2.3 {join errors} {
#    list [catch {join "a \{ c" 111} msg] $msg
#} {1 {unmatched open brace in list}}

test join-3.1 {joinString is binary ok} {
  string length [join {a b c} a\0b]
} 9

test join-3.2 {join is binary ok} {
  string length [join "a\0b a\0b a\0b"]
} 11

################################################################################
# SWITCH
################################################################################

test switch-1.1 {simple patterns} {
    switch a a {expr 1} b {expr 2} c {expr 3} default {expr 4}
} 1
test switch-1.2 {simple patterns} {
    switch b a {expr 1} b {expr 2} c {expr 3} default {expr 4}
} 2
test switch-1.3 {simple patterns} {
    switch x a {expr 1} b {expr 2} c {expr 3} default {expr 4}
} 4
test switch-1.4 {simple patterns} {
    switch x a {expr 1} b {expr 2} c {expr 3}
} {}
test switch-1.5 {simple pattern matches many times} {
    switch b a {expr 1} b {expr 2} b {expr 3} b {expr 4}
} 2
test switch-1.6 {simple patterns} {
    switch default a {expr 1} default {expr 2} c {expr 3} default {expr 4}
} 2
test switch-1.7 {simple patterns} {
    switch x a {expr 1} default {expr 2} c {expr 3} default {expr 4}
} 4

test switch-2.1 {single-argument form for pattern/command pairs} {
    switch b {
	a {expr 1}
	b {expr 2}
	default {expr 6}
    }
} {2}
test switch-2.2 {single-argument form for pattern/command pairs} {
    list [catch {switch z {a 2 b}}]
} 1

test switch-3.1 {-exact vs. -glob vs. -regexp} {
    switch -exact aaaab {
	^a*b$	{concat regexp}
	*b	{concat glob}
	aaaab	{concat exact}
	default	{concat none}
    }
} exact
test switch-3.2 {-exact vs. -glob vs. -regexp (no [regexp] cmd)} {
    catch {
        switch -regexp aaaab {
        ^a*b$	{concat regexp}
        *b	{concat glob}
        aaaab	{concat exact}
        default	{concat none}
        }
    }
} 1
test switch-3.3 {-exact vs. -glob vs. -regexp (with [regexp] cmd)} {
    proc regexp {pat str} {expr {$pat eq "^a*b$" && $str eq "aaaab"}}
    switch -regexp aaaab {
	^a*b$	{concat regexp}
	*b	    {concat glob}
	aaaab	{concat exact}
	default	{concat none}
    }
} regexp
test switch-3.4 {-exact vs. -glob vs. -regexp} {
    switch -glob aaaab {
	^a*b$	{concat regexp}
	*b	    {concat glob}
	aaaab	{concat exact}
	default	{concat none}
    }
} glob
test switch-3.5 {-exact vs. -glob vs. -regexp} {
    switch aaaab {^a*b$} {concat regexp} *b {concat glob} \
	    aaaab {concat exact} default {concat none}
} exact
test switch-3.6 {-exact vs. -glob vs. -regexp} {
    switch -- -glob {
	^g.*b$	{concat regexp}
	-*	{concat glob}
	-glob	{concat exact}
	default {concat none}
    }
} exact
test switch-3.7 {-exact vs. -glob vs. -regexp} {
    list [catch {switch -foo a b c} msg] $msg
} {1 {bad option "-foo": must be -exact, -glob, -regexp, -command procname or --}}

test switch-4.1 {error in executed command} {
    list [catch {switch a a {error "Just a test"} default {expr 1}} msg] \
	    $msg
} {1 {Just a test}}
test switch-4.2 {error: not enough args} {
    catch {switch}
} 1
test switch-4.3 {error: pattern with no body} {
    catch {switch a b}
} 1
test switch-4.4 {error: pattern with no body} {
    catch {switch a b {expr 1} c}
} 1
test switch-4.5 {error in default command} {
    list [catch {switch foo a {error switch1} b {error switch 3} \
	    default {error switch2}} msg] $msg
} {1 switch2}

#~ test switch-5.1 {errors in -regexp matching} {
    #~ list [catch {switch -regexp aaaab {
	#~ *b	{concat glob}
	#~ aaaab	{concat exact}
	#~ default	{concat none}
    #~ }} msg] $msg
#~ } {1 {couldn't compile regular expression pattern: quantifier operand invalid}}

test switch-6.1 {backslashes in patterns} {
    switch -exact {\a\$\.\[} {
	\a\$\.\[	{concat first}
	\a\\$\.\\[	{concat second}
	\\a\\$\\.\\[	{concat third}
	{\a\\$\.\\[}	{concat fourth}
	{\\a\\$\\.\\[}	{concat fifth}
	default		{concat none}
    }
} third
test switch-6.2 {backslashes in patterns} {
    switch -exact {\a\$\.\[} {
	\a\$\.\[	{concat first}
	{\a\$\.\[}	{concat second}
	{{\a\$\.\[}}	{concat third}
	default		{concat none}
    }
} second

test switch-7.1 {"-" bodies} {
    switch a {
	a -
	b -
	c {concat 1}
	default {concat 2}
    }
} 1
test switch-7.2 {"-" bodies} {
    list [catch {
	switch a {
	    a -
	    b -
	    c -
	}
    } msg] $msg
} {1 {no body specified for pattern "c"}}
# Following original Tcl test makes no sense, I feel! Please review ...
#~ test switch-7.3 {"-" bodies} {
    #~ list [catch {
	#~ switch a {
	    #~ a -
	    #~ b -foo
	    #~ c -
	#~ }
    #~ } msg] $msg
#~ } {1 {no body specified for pattern "c"}}
test switch-7.3 {"-" bodies} {
    list [catch {
	switch a {
	    a -
	    b -foo
	    c -
	}
    } msg] $msg
} {1 {invalid command name "-foo"}}

test switch-8.1 {empty body} {
    set msg {}
    switch {2} {
    	1 {set msg 1}
        2 {}
        default {set msg 2}
    }
} {}

test switch-9.1 {empty pattern/body list} {
    catch {switch x}
} 1
test switch-9.2 {empty pattern/body list} {
    catch {switch -- x} 
} 1 
test switch-9.3 {empty pattern/body list} {
    catch {switch x {}} 
} 1
test switch-9.4 {empty pattern/body list} {
    catch {switch -- x {}}
} 1
test switch-9.5 {unpaired pattern} {
    catch {switch x a {} b}
} 1
test switch-9.6 {unpaired pattern} {
    catch {switch x {a {} b}}
} 1
test switch-9.7 {unpaired pattern} {
    catch {switch x a {} # comment b}
} 1
test switch-9.8 {unpaired pattern} {
    catch {switch x {a {} # comment b}}
} 1
test switch-9.9 {unpaired pattern} {
    catch {switch x a {} x {} # comment b}
} 1
test switch-9.10 {unpaired pattern} {
    catch {switch x {a {} x {} # comment b}}
} 1

test switch-10.1 {no callback given to -command} {
    catch {switch -command a { a {expr 1} b {expr 2} }} 
} 1
test switch-10.2 {callback expect wrong # args for -command} {
    catch {switch -command [lambda {p1} {expr 1}] a { a {expr 1} b {expr 2} }}
} 1
test switch-10.3 {callback to -command returns ever 0: no match} {
    switch -command [lambda {p1 p2} {expr 0}] a a {expr 1} b {expr 2}
} {}
test switch-10.4 {callback to -command returns 3 at first match} {
    switch -command [lambda {p1 p2} {expr 3}] a a {expr 1} b {expr 2}
} 1
test switch-10.5 {[error] in callback to -command} {
    list [catch {
        switch -command [lambda {p1 p2} {error "foo"}] a a {expr 1} b {expr 2}
    } msg] $msg
} {1 foo}
test switch-10.6 {[continue] in callback to -command} {
    list [catch {
        switch -command [lambda {p1 p2} {continue}] a a {expr 1} b {expr 2}
    } msg] $msg
} {4 {}}
test switch-10.7 {callback matches first if pat < str} {
    switch -command [lambda {pat str} {expr {$pat < $str}}] 3 \
        5 {expr 1} 3 {expr 2}
} {}
test switch-10.8 {callback matches first if pat < str} {
    switch -command [lambda {pat str} {expr {$pat < $str}}] 7 \
        5 {expr 1} 3 {expr 2}
} 1
test switch-10.9 {callback matches first if pat < str} {
    switch -command [lambda {pat str} {expr {$pat < $str}}] 4 \
        5 {expr 1} 3 {expr 2}
} 2

################################################################################
# FOR
################################################################################

# Basic "for" operation.

test for-1.1 {TclCompileForCmd: missing initial command} {
    list [catch {for} msg] $msg
} {1 {wrong # args: should be "for start test next body"}}
test for-1.2 {TclCompileForCmd: error in initial command} {
    list [catch {for {set}} msg] $msg
} {1 {wrong # args: should be "for start test next body"}}
catch {unset i}
test for-1.3 {TclCompileForCmd: missing test expression} {
    catch {for {set i 0}} msg
    set msg
} {wrong # args: should be "for start test next body"}
test for-1.5 {TclCompileForCmd: test expression is enclosed in quotes} {
    set i 0
    for {} "$i > 5" {incr i} {}
} {}
test for-1.6 {TclCompileForCmd: missing "next" command} {
    catch {for {set i 0} {$i < 5}} msg
    set msg
} {wrong # args: should be "for start test next body"}
test for-1.7 {TclCompileForCmd: missing command body} {
    catch {for {set i 0} {$i < 5} {incr i}} msg
    set msg
} {wrong # args: should be "for start test next body"}
catch {unset a}
test for-1.9 {TclCompileForCmd: simple command body} {
    set a {}
    for {set i 1} {$i<6} {set i [expr $i+1]} {
	if $i==4 break
	set a [concat $a $i]
    }
    set a
} {1 2 3}
test for-1.10 {TclCompileForCmd: command body in quotes} {
    set a {}
    for {set i 1} {$i<6} {set i [expr $i+1]} "append a x"
    set a
} {xxxxx}
test for-1.11 {TclCompileForCmd: computed command body} {
    catch {unset x1}
    catch {unset bb}
    catch {unset x2}
    set x1 {append a x1; }
    set bb {break}
    set x2 {; append a x2}
    set a {}
    for {set i 1} {$i<6} {set i [expr $i+1]} $x1$bb$x2
    set a
} {x1}
test for-1.13 {TclCompileForCmd: long command body} {
    set a {}
    for {set i 1} {$i<6} {set i [expr $i+1]} {
	if $i==4 break
	if $i>5 continue
        set tcl_platform(machine) i686
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	set a [concat $a $i]
    }
    set a
} {1 2 3}
test for-1.14 {TclCompileForCmd: for command result} {
    set a [for {set i 0} {$i < 5} {incr i} {}]
    set a
} {}
test for-1.15 {TclCompileForCmd: for command result} {
    set a [for {set i 0} {$i < 5} {incr i} {if $i==3 break}]
    set a
} {}

# Check "for" and "continue".

test for-2.1 {TclCompileContinueCmd: arguments after "continue"} {
    catch {continue foo} msg
    set msg
} {wrong # args: should be "continue"}
test for-2.2 {TclCompileContinueCmd: continue result} {
    catch continue
} 4
test for-2.3 {continue tests} {
    set a {}
    for {set i 1} {$i <= 4} {set i [expr $i+1]} {
	if {$i == 2} continue
	set a [concat $a $i]
    }
    set a
} {1 3 4}
test for-2.4 {continue tests} {
    set a {}
    for {set i 1} {$i <= 4} {set i [expr $i+1]} {
	if {$i != 2} continue
	set a [concat $a $i]
    }
    set a
} {2}
test for-2.5 {continue tests, nested loops} {
    set msg {}
    for {set i 1} {$i <= 4} {incr i} {
	for {set a 1} {$a <= 2} {incr a} {
            if {$i>=2 && $a>=2} continue
            set msg [concat $msg "$i.$a"]
        }
    }
    set msg
} {1.1 1.2 2.1 3.1 4.1}
test for-2.6 {continue tests, long command body} {
    set a {}
    for {set i 1} {$i<6} {set i [expr $i+1]} {
	if $i==2 continue
	if $i==4 break
	if $i>5 continue
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	set a [concat $a $i]
    }
    set a
} {1 3}

# Check "for" and "break".

test for-3.1 {TclCompileBreakCmd: arguments after "break"} {
    catch {break foo} msg
    set msg
} {wrong # args: should be "break"}
test for-3.2 {TclCompileBreakCmd: break result} {
    catch break
} 3
test for-3.3 {break tests} {
    set a {}
    for {set i 1} {$i <= 4} {incr i} {
	if {$i == 3} break
	set a [concat $a $i]
    }
    set a
} {1 2}
test for-3.4 {break tests, nested loops} {
    set msg {}
    for {set i 1} {$i <= 4} {incr i} {
	for {set a 1} {$a <= 2} {incr a} {
            if {$i>=2 && $a>=2} break
            set msg [concat $msg "$i.$a"]
        }
    }
    set msg
} {1.1 1.2 2.1 3.1 4.1}
test for-3.5 {break tests, long command body} {
    set a {}
    for {set i 1} {$i<6} {set i [expr $i+1]} {
	if $i==2 continue
	if $i==5 break
	if $i>5 continue
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i == 4} break
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	set a [concat $a $i]
    }
    set a
} {1 3}
test for-4.1 {break must reset the interp result} {
    catch {
        set z GLOBTESTDIR/dir2/file2.c
        if [string match GLOBTESTDIR/dir2/* $z] {
            break
        }
    } j
    set j
} {}

# Test for incorrect "double evaluation" semantics

test for-5.1 {possible delayed substitution of increment command} {
    # Increment should be 5, and lappend should always append $a
    catch {unset a}
    catch {unset i}
    set a 5
    set i {}
    for {set a 1} {$a < 12} "incr a $a" {lappend i $a}
    set i
} {1 6 11}

test for-5.2 {possible delayed substitution of increment command} {
    # Increment should be 5, and lappend should always append $a
    catch {rename p ""}
    proc p {} {
	set a 5
	set i {}
	for {set a 1} {$a < 12} "incr a $a" {lappend i $a}
	set i
    }
    p
} {1 6 11}
test for-5.3 {possible delayed substitution of body command} {
    # Increment should be $a, and lappend should always append 5
    set a 5
    set i {}
    for {set a 1} {$a < 12} {incr a $a} "lappend i $a"
    set i
} {5 5 5 5}
test for-5.4 {possible delayed substitution of body command} {
    # Increment should be $a, and lappend should always append 5
    catch {rename p ""}
    proc p {} {
	set a 5
	set i {}
	for {set a 1} {$a < 12} {incr a $a} "lappend i $a"
	set i
    }
    p
} {5 5 5 5}

# In the following tests we need to bypass the bytecode compiler by
# substituting the command from a variable.  This ensures that command
# procedure is invoked directly.

test for-6.1 {Tcl_ForObjCmd: number of args} {
    set z for
    catch {$z} msg
    set msg
} {wrong # args: should be "for start test next body"}
test for-6.2 {Tcl_ForObjCmd: number of args} {
    set z for
    catch {$z {set i 0}} msg
    set msg
} {wrong # args: should be "for start test next body"}
test for-6.3 {Tcl_ForObjCmd: number of args} {
    set z for
    catch {$z {set i 0} {$i < 5}} msg
    set msg
} {wrong # args: should be "for start test next body"}
test for-6.4 {Tcl_ForObjCmd: number of args} {
    set z for
    catch {$z {set i 0} {$i < 5} {incr i}} msg
    set msg
} {wrong # args: should be "for start test next body"}
test for-6.5 {Tcl_ForObjCmd: number of args} {
    set z for
    catch {$z {set i 0} {$i < 5} {incr i} {body} extra} msg
    set msg
} {wrong # args: should be "for start test next body"}
test for-6.6 {Tcl_ForObjCmd: error in initial command} {
    set z for
    list [catch {$z {set} {$i < 5} {incr i} {body}} msg] $msg
} {1 {wrong # args: should be "set varName ?newValue?"}}
test for-6.8 {Tcl_ForObjCmd: test expression is enclosed in quotes} {
    set z for
    set i 0
    $z {set i 6} "$i > 5" {incr i} {set y $i}
    set i
} 6
test for-6.10 {Tcl_ForObjCmd: simple command body} {
    set z for
    set a {}
    $z {set i 1} {$i<6} {set i [expr $i+1]} {
	if $i==4 break
	set a [concat $a $i]
    }
    set a
} {1 2 3}
test for-6.11 {Tcl_ForObjCmd: command body in quotes} {
    set z for
    set a {}
    $z {set i 1} {$i<6} {set i [expr $i+1]} "append a x"
    set a
} {xxxxx}
test for-6.12 {Tcl_ForObjCmd: computed command body} {
    set z for
    catch {unset x1}
    catch {unset bb}
    catch {unset x2}
    set x1 {append a x1; }
    set bb {break}
    set x2 {; append a x2}
    set a {}
    $z {set i 1} {$i<6} {set i [expr $i+1]} $x1$bb$x2
    set a
} {x1}
test for-6.14 {Tcl_ForObjCmd: long command body} {
    set z for
    set a {}
    $z {set i 1} {$i<6} {set i [expr $i+1]} {
	if $i==4 break
	if $i>5 continue
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	if {$i>6 && $tcl_platform(machine) eq "xxx"} {
	    catch {set a $a} msg
	    catch {incr i 5} msg
	    catch {incr i -5} msg
	}
	set a [concat $a $i]
    }
    set a
} {1 2 3}
test for-6.15 {Tcl_ForObjCmd: for command result} {
    set z for
    set a [$z {set i 0} {$i < 5} {incr i} {}]
    set a
} {}
test for-6.16 {Tcl_ForObjCmd: for command result} {
    set z for
    set a [$z {set i 0} {$i < 5} {incr i} {if $i==3 break}]
    set a
} {}

################################################################################
# INFO
################################################################################

test info-1.1 {info body option} {
    proc t1 {} {body of t1}
    info body t1
} {body of t1}
test info-1.2 {info body option} {
    list [catch {info body set} msg] $msg
} {1 {command "set" is not a procedure}}
#~ test info-1.3 {info body option} {
    #~ list [catch {info args set 1} msg] $msg
#~ } {1 {wrong # args: should be "info args procname"}}
test info-1.5 {info body option, returning bytecompiled bodies} {
    catch {unset args}
    proc foo {args} {
        foreach v $args {
            upvar $v var
            return "variable $v existence: [info exists var]"
        }
    }
    foo a
    list [catch [info body foo] msg] $msg
} {1 {can't read "args": no such variable}}
#~ test info-1.6 {info body option, returning list bodies} {
    #~ proc foo args [list subst bar]
    #~ list [string bytelength [info body foo]] \
	    #~ [foo; string bytelength [info body foo]]
#~ } {9 9}
test info-2.1 {info commands option} {
    proc t1 {} {}
    proc t2 {} {}
    set x " [info commands] "
    list [string match {* t1 *} $x] [string match {* t2 *} $x] \
            [string match {* set *} $x] [string match {* list *} $x]
} {1 1 1 1}
test info-2.2 {info commands option} {
    proc t1 {} {}
    rename t1 {}
    set x [info commands]
    string match {* t1 *} $x
} 0
test info-2.3 {info commands option} {
    proc _t1_ {} {}
    proc _t2_ {} {}
    info commands _t1_
} _t1_
test info-2.4 {info commands option} {
    proc _t1_ {} {}
    proc _t2_ {} {}
    lsort [info commands _t*]
} {_t1_ _t2_}
catch {rename _t1_ {}}
catch {rename _t2_ {}}
test info-2.5 {info commands option} {
    list [catch {info commands a b} msg] $msg
} {1 {wrong # args: should be "info commands ?pattern?"}}
test info-3.1 {info exists option} {
    set value foo
    info exists value
} 1
catch {unset _nonexistent_}
test info-3.2 {info exists option} {
    info exists _nonexistent_
} 0
test info-3.3 {info exists option} {
    proc t1 {x} {return [info exists x]}
    t1 2
} 1
test info-3.4 {info exists option} {
    proc t1 {x} {
        global _nonexistent_
        return [info exists _nonexistent_]
    }
    t1 2
} 0
test info-3.5 {info exists option} {
    proc t1 {x} {
        set y 47
        return [info exists y]
    }
    t1 2
} 1
test info-3.6 {info exists option} {
    proc t1 {x} {return [info exists value]}
    t1 2
} 0
test info-3.7 {info exists option} {
    catch {unset x}
    set x(2) 44
    list [info exists x] [info exists x(1)] [info exists x(2)]
} {1 0 1}
catch {unset x}
test info-3.8 {info exists option} {
    list [catch {info exists} msg] $msg
} {1 {wrong # args: should be "info exists varName"}}
test info-3.9 {info exists option} {
    list [catch {info exists 1 2} msg] $msg
} {1 {wrong # args: should be "info exists varName"}}
test info-4.1 {info globals option} {
    set x 1
    set y 2
    set value 23
    set a " [info globals] "
    list [string match {* x *} $a] [string match {* y *} $a] \
            [string match {* value *} $a] [string match {* _foobar_ *} $a]
} {1 1 1 0}
test info-4.2 {info globals option} {
    set _xxx1 1
    set _xxx2 2
    lsort [info globals _xxx*]
} {_xxx1 _xxx2}
test info-4.3 {info globals option} {
    list [catch {info globals 1 2} msg] $msg
} {1 {wrong # args: should be "info globals ?pattern?"}}
test info-5.1 {info level option} {
    info level
} 0
#~ test info-5.2 {info level option} {
    #~ proc t1 {a b} {
        #~ set x [info level]
        #~ set y [info level 1]
        #~ list $x $y
    #~ }
    #~ t1 146 testString
#~ } {1 {t1 146 testString}}
#~ test info-5.3 {info level option} {
    #~ proc t1 {a b} {
        #~ t2 [expr $a*2] $b
    #~ }
    #~ proc t2 {x y} {
        #~ list [info level] [info level 1] [info level 2] [info level -1] \
                #~ [info level 0]
    #~ }
    #~ t1 146 {a {b c} {{{c}}}}
#~ } {2 {t1 146 {a {b c} {{{c}}}}} {t2 292 {a {b c} {{{c}}}}} {t1 146 {a {b c} {{{c}}}}} {t2 292 {a {b c} {{{c}}}}}}
#~ test info-5.4 {info level option} {
    #~ proc t1 {} {
        #~ set x [info level]
        #~ set y [info level 1]
        #~ list $x $y
    #~ }
    #~ t1
#~ } {1 t1}
test info-5.5 {info level option} {
    list [catch {info level 1 2} msg] $msg
} {1 {wrong # args: should be "info level ?levelNum?"}}
test info-5.6 {info level option} {
    list [catch {info level 123a} msg] $msg
} {1 {bad level "123a"}}
test info-5.7 {info level option} {
    list [catch {info level 0} msg] $msg
} {1 {bad level "0"}}
test info-5.8 {info level option} {
    proc t1 {} {info level -1}
    list [catch {t1} msg] $msg
} {1 {bad level "-1"}}
test info-5.9 {info level option} {
    proc t1 {x} {info level $x}
    list [catch {t1 -3} msg] $msg
} {1 {bad level "-3"}}
test info-6.1 {info locals option} {
    set a 22
    proc t1 {x y} {
        set b 13
        set c testing
        global a
	global aa
	set aa 23
        return [info locals]
    }
    lsort [t1 23 24]
} {b c x y}
test info-6.2 {info locals option} {
    proc t1 {x y} {
        set xx1 2
        set xx2 3
        set y 4
        return [info locals x*]
    }
    lsort [t1 2 3]
} {x xx1 xx2}
test info-6.3 {info locals option} {
    list [catch {info locals 1 2} msg] $msg
} {1 {wrong # args: should be "info locals ?pattern?"}}
test info-6.4 {info locals option} {
    info locals
} {}
test info-6.5 {info locals option} {
    proc t1 {} {return [info locals]}
    t1
} {}
test info-6.6 {info locals vs unset compiled locals} {
    proc t1 {lst} {
        foreach $lst $lst {}
        unset lst
        return [info locals]
    }
    lsort [t1 {a b c c d e f}]
} {a b c d e f}
test info-6.7 {info locals with temporary variables} {
    proc t1 {} {
        foreach a {b c} {}
        info locals
    }
    t1
} {a}
test info-7.1 {info vars option} {
    set a 1
    set b 2
    proc t1 {x y} {
        global a b
        set c 33
        return [info vars]
    }
    lsort [t1 18 19]
} {a b c x y}
test info-7.2 {info vars option} {
    set xxx1 1
    set xxx2 2
    proc t1 {xxa y} {
        global xxx1 xxx2
        set c 33
        return [info vars x*]
    }
    lsort [t1 18 19]
} {xxa xxx1 xxx2}
test info-7.3 {info vars option} {
    lsort [info vars]
} [lsort [info globals]]
test info-7.4 {info vars option} {
    list [catch {info vars a b} msg] $msg
} {1 {wrong # args: should be "info vars ?pattern?"}}
test info-7.5 {info vars with temporary variables} {
    proc t1 {} {
        foreach a {b c} {}
        info vars
    }
    t1
} {a}


################################################################################
# linsert
################################################################################

test linsert-1.1 {linsert command} {
    linsert {1 2 3 4 5} 0 a
} {a 1 2 3 4 5}
test linsert-1.2 {linsert command} {
    linsert {1 2 3 4 5} 1 a
} {1 a 2 3 4 5}
test linsert-1.3 {linsert command} {
    linsert {1 2 3 4 5} 2 a
} {1 2 a 3 4 5}
test linsert-1.4 {linsert command} {
    linsert {1 2 3 4 5} 3 a
} {1 2 3 a 4 5}
test linsert-1.5 {linsert command} {
    linsert {1 2 3 4 5} 4 a
} {1 2 3 4 a 5}
test linsert-1.6 {linsert command} {
    linsert {1 2 3 4 5} 5 a
} {1 2 3 4 5 a}
test linsert-1.7 {linsert command} {
    linsert {1 2 3 4 5} 2 one two \{three \$four
} {1 2 one two \{three {$four} 3 4 5}
test linsert-1.8 {linsert command} {
    linsert {\{one \$two \{three \ four \ five} 2 a b c
} {\{one {$two} a b c \{three { four} { five}}
test linsert-1.9 {linsert command} {
    linsert {{1 2} {3 4} {5 6} {7 8}} 2 {x y} {a b}
} {{1 2} {3 4} {x y} {a b} {5 6} {7 8}}
test linsert-1.10 {linsert command} {
    linsert {} 2 a b c
} {a b c}
test linsert-1.11 {linsert command} {
    linsert {} 2 {}
} {{}}
test linsert-1.12 {linsert command} {
    linsert {a b "c c" d e} 3 1
} {a b {c c} 1 d e}
test linsert-1.13 {linsert command} {
    linsert { a b c d} 0 1 2
} {1 2 a b c d}
test linsert-1.14 {linsert command} {
    linsert {a b c {d e f}} 4 1 2
} {a b c {d e f} 1 2}
test linsert-1.15 {linsert command} {
    linsert {a b c \{\  abc} 4 q r
} {a b c \{\  q r abc}
test linsert-1.16 {linsert command} {
    linsert {a b c \{ abc} 4 q r
} {a b c \{ q r abc}
test linsert-1.17 {linsert command} {
    linsert {a b c} end q r
} {a b c q r}
test linsert-1.18 {linsert command} {
    linsert {a} end q r
} {a q r}
test linsert-1.19 {linsert command} {
    linsert {} end q r
} {q r}
test linsert-1.20 {linsert command, use of end-int index} {
    linsert {a b c d} end-2 e f
} {a b e f c d}

test linsert-2.1 {linsert errors} {
    list [catch linsert msg] $msg
} {1 {wrong # args: should be "linsert list index element ?element ...?"}}
test linsert-2.2 {linsert errors} {
    list [catch {linsert a b} msg] $msg
} {1 {wrong # args: should be "linsert list index element ?element ...?"}}
test linsert-2.3 {linsert errors} {
    list [catch {linsert a 12x 2} msg] $msg
} {1 {bad index "12x": must be integer or end?-integer?}}

test linsert-3.1 {linsert won't modify shared argument objects} {
    proc p {} {
        linsert "a b c" 1 "x y"
        return "a b c"
    }
    p
} "a b c"
test linsert-3.2 {linsert won't modify shared argument objects} {
    catch {unset lis}
    set lis [concat a \"b\" c]
    linsert $lis 0 [string length $lis]
} "7 a b c"

################################################################################
# LRANGE
################################################################################

test lrange-1.1 {range of list elements} {
    lrange {a b c d} 1 2
} {b c}
test lrange-1.2 {range of list elements} {
    lrange {a {bcd e {f g {}}} l14 l15 d} 1 1
} {{bcd e {f g {}}}}
test lrange-1.3 {range of list elements} {
    lrange {a {bcd e {f g {}}} l14 l15 d} 3 end
} {l15 d}
test lrange-1.4 {range of list elements} {
    lrange {a {bcd e {f g {}}} l14 l15 d} 4 10000
} {d}
test lrange-1.5 {range of list elements} {
    lrange {a {bcd e {f g {}}} l14 l15 d} 4 3
} {}
test lrange-1.6 {range of list elements} {
    lrange {a {bcd e {f g {}}} l14 l15 d} 10 11
} {}
test lrange-1.7 {range of list elements} {
    lrange {a b c d e} -1 2
} {a b c}
test lrange-1.8 {range of list elements} {
    lrange {a b c d e} -2 -1
} {}
#test lrange-1.9 {range of list elements} {
#    lrange {a b c d e} -2 e
#} {a b c d e}
test lrange-1.10 {range of list elements} {
    lrange "a b\{c d" 1 2
} "b\\{c d"
test lrange-1.11 {range of list elements} {
    lrange "a b c d" end end
} d
test lrange-1.12 {range of list elements} {
    lrange "a b c d" end 100000
} d
#test lrange-1.13 {range of list elements} {
#    lrange "a b c d" e 3
#} d
test lrange-1.14 {range of list elements} {
    lrange "a b c d" end 2
} {}
test lrange-1.15 {range of list elements} {
    concat \"[lrange {a b \{\   	} 0 2]"
} {"a b \{\ "}
test lrange-1.16 {list element quoting} {
    lrange {[append a .b]} 0 end    
} {{[append} a .b\]}

test lrange-2.1 {error conditions} {
    list [catch {lrange a b} msg] $msg
} {1 {wrong # args: should be "lrange list first last"}}
test lrange-2.2 {error conditions} {
    list [catch {lrange a b 6 7} msg] $msg
} {1 {wrong # args: should be "lrange list first last"}}
test lrange-2.3 {error conditions} {
    list [catch {lrange a b 6} msg] $msg
} {1 {bad index "b": must be integer or end?-integer?}}
test lrange-2.4 {error conditions} {
    list [catch {lrange a 0 enigma} msg] $msg
} {1 {bad index "enigma": must be integer or end?-integer?}}
#test lrange-2.5 {error conditions} {
#    list [catch {lrange "a \{b c" 3 4} msg] $msg
#} {1 {unmatched open brace in list}}
#test lrange-2.6 {error conditions} {
#    list [catch {lrange "a b c \{ d e" 1 4} msg] $msg
#} {1 {unmatched open brace in list}}

################################################################################
# FINAL REPORT
################################################################################

puts "----------------------------------------------------------------------"
puts "FAILED: $failedTests"
puts "PASSED: $passedTests"
puts "----------------------------------------------------------------------\n"
