proc t {an v} {
	upvar $an a
	return $a($v)
}

proc t2 {anv} {
	upvar $anv av
	return $av
}

array set a {b B c C}

set res [t a b]
check upvar.array.1 $res B

set res [t a c]
check upvar.array.2 $res C

set res [t2 a(b)]
check upvar.array.3 $res B

set res [t2 a(c)]
check upvar.array.4 $res C
