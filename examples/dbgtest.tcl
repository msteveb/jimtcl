# An example script useful for testing the Jim debugger
# Taken from http://www.nist.gov/msidlibrary/doc/libes93c.ps

set b 1

proc p4 {x} {
    return [
        expr 5+[expr 1+$x]]
}

set z [
    expr 1+[expr 2+[p4 $b]]
]

proc p3 {} {
    set m 0
}

proc p2 {} {
    set c 4
    p3
    set d 5
}

proc p1 {} {
    set a 2
    p2
    set a 3
    set a 5
}

p1
set k 7
p1
