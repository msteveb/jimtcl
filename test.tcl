proc test {id descr script expectedResult} {
    puts -nonewline "$id $descr: "
    set result [uplevel 1 $script]
    if {$result eq $expectedResult} {
	puts "OK"
    } else {
	puts "ERR"
	puts "Expected: '$expectedResult'"
	puts "Got     : '$result'"
	exit 1
    }
}

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
# STRING MATCH
################################################################################

test string-11.3 {string match} {
    string match abc abc
} 1
test string-11.5 {string match} {
    string match ab*c abc
} 1
test string-11.6 {string match} {
    string match ab**c abc
} 1
test string-11.7 {string match} {
    string match ab* abcdef
} 1
test string-11.8 {string match} {
    string match *c abc
} 1
test string-11.9 {string match} {
    string match *3*6*9 0123456789
} 1
test string-11.10 {string match} {
    string match *3*6*9 01234567890
} 0
test string-11.11 {string match} {
    string match a?c abc
} 1
test string-11.12 {string match} {
    string match a??c abc
} 0
test string-11.13 {string match} {
    string match ?1??4???8? 0123456789
} 1
test string-11.14 {string match} {
    string match {[abc]bc} abc
} 1
test string-11.15 {string match} {
    string match {a[abc]c} abc
} 1
test string-11.16 {string match} {
    string match {a[xyz]c} abc
} 0
test string-11.17 {string match} {
    string match {12[2-7]45} 12345
} 1
test string-11.18 {string match} {
    string match {12[ab2-4cd]45} 12345
} 1
test string-11.19 {string match} {
    string match {12[ab2-4cd]45} 12b45
} 1
test string-11.20 {string match} {
    string match {12[ab2-4cd]45} 12d45
} 1
test string-11.21 {string match} {
    string match {12[ab2-4cd]45} 12145
} 0
test string-11.22 {string match} {
    string match {12[ab2-4cd]45} 12545
} 0
test string-11.23 {string match} {
    string match {a\*b} a*b
} 1
test string-11.24 {string match} {
    string match {a\*b} ab
} 0
test string-11.25 {string match} {
    string match {a\*\?\[\]\\\x} "a*?\[\]\\x"
} 1
test string-11.26 {string match} {
    string match ** ""
} 1
test string-11.27 {string match} {
    string match *. ""
} 0
test string-11.28 {string match} {
    string match "" ""
} 1
test string-11.29 {string match} {
    string match \[a a
} 1
test string-11.31 {string match case} {
    string match a A
} 0
test string-11.34 {string match nocase} {
    string match -nocase a*f ABCDEf
} 1
test string-11.35 {string match case, false hope} {
    # This is true because '_' lies between the A-Z and a-z ranges
    string match {[A-z]} _
} 1
