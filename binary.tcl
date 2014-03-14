# Implements the 'binary scan' and 'binary format' commands.
#
# (c) 2010 Steve Bennett <steveb@workware.net.au>
#
# See LICENCE in this directory for licensing.

package require pack
package require regexp

proc binary {cmd args} {
	tailcall "binary $cmd" {*}$args
}

proc "binary format" {formatString args} {
	set bitoffset 0
	set result {}
	# This RE is too unreliable...
	foreach {conv t u n} [regexp -all -inline {([^[:space:]])(u)?([*0-9]*)} $formatString] {
		switch -exact -- $t {
			a -
			A {
				set value [binary::nextarg args]
				set sn [string bytelength $value]
				if {$n ne "*"} {
					if {$n eq ""} {
						set n 1
					}
					if {$n > $sn} {
						# Need to pad the string with spaces or nulls
						append value [string repeat [dict get {A " " a \x00} $t] $($n - $sn)]
					}
				} else {
					set n $sn
				}
				if {$n} {
					set bitoffset [pack result $value -str $(8 * $n) $bitoffset]
				}
			}
			x {
				if {$n eq "*"} {
					return -code error {cannot use "*" in format string with "x"}
				}
				if {$n eq ""} {
					set n 1
				}
				loop i 0 $n {
					set bitoffset [pack result 0 -intbe 8 $bitoffset]
				}
			}
			@ {
				if {$n eq ""} {
					return -code error {missing count for "@" field specifier}
				}
				if {$n eq "*"} {
					set bitoffset $(8 * [string bytelength $result])
				} else {
					# May need to pad it out
					set max [string bytelength $result]
					append result [string repeat \x00 $($n - $max)]
					set bitoffset $(8 * $n)
				}
			}
			X {
				if {$n eq "*"} {
					set bitoffset 0
				} elseif {$n eq ""} {
					incr bitoffset -8
				} else {
					incr bitoffset $($n * -8)
				}
				if {$bitoffset < 0} {
					set bitoffset 0
				}
			}
			default {
				if {![info exists ::binary::scalarinfo($t)]} {
					return -code error "bad field specifier \"$t\""
				}

				# A scalar (integer or float) type
				lassign $::binary::scalarinfo($t) type convtype size prefix
				set value [binary::nextarg args]

				if {$type in {bin hex}} {
					set value [split $value {}]
				}
				set vn [llength $value]
				if {$n eq "*"} {
					set n $vn
				} elseif {$n eq ""} {
					set n 1
					set value [list $value]
				} elseif {$vn < $n} {
					if {$type in {bin hex}} {
						# Need to pad the list with zeros
						lappend value {*}[lrepeat $($n - $vn) 0]
					} else {
						return -code error "number of elements in list does not match count"
					}
				} elseif {$vn > $n} {
					# Need to truncate the list
					set value [lrange $value 0 $n-1]
				}

				set convtype -$::binary::convtype($convtype)

				foreach v $value {
					set bitoffset [pack result $prefix$v $convtype $size $bitoffset]
				}
				# Now pad out with zeros to the end of the current byte
				if {$bitoffset % 8} {
					set bitoffset [pack result 0 $convtype $(8 - $bitoffset % 8) $bitoffset]
				}
			}
		}
	}
	return $result
}

proc "binary scan" {value formatString {args varName}} {
	# Pops the next arg from the front of the list and returns it.
	# Throws an error if no more args
	set bitoffset 0
	set count 0
	# This RE is too unreliable...
	foreach {conv t u n} [regexp -all -inline {([^[:space:]])(u)?([*0-9]*)} $formatString] {
		set rembytes $([string bytelength $value] - $bitoffset / 8)
		switch -exact -- $t {
			a -
			A {
				if {$n eq "*"} {
					set n $rembytes
				} elseif {$n eq ""} {
					set n 1
				}
				if {$n > $rembytes} {
					break
				}

				set var [binary::nextarg varName]

				set result [unpack $value -str $bitoffset $($n * 8)]
				incr bitoffset $([string bytelength $result] * 8)
				if {$t eq "A"} {
					set result [string trimright $result]
				}
			}
			x {
				# Skip bytes
				if {$n eq "*"} {
					set n $rembytes
				} elseif {$n eq ""} {
					set n 1
				}
				if {$n > $rembytes} {
					set n $rembytes
				}
				incr bitoffset $($n * 8)
				continue
			}
			X {
				# Back up bytes
				if {$n eq "*"} {
					set bitoffset 0
					continue
				}
				if {$n eq ""} {
					set n 1
				}
				if {$n * 8 > $bitoffset} {
					set bitoffset 0
					continue
				}
				incr bitoffset -$($n * 8)
				continue
			}
			@ {
				if {$n eq ""} {
					return -code error {missing count for "@" field specifier}
				}
				if {$n eq "*" || $n > $rembytes + $bitoffset / 8} {
					incr bitoffset $($rembytes * 8)
				} elseif {$n < 0} {
					set bitoffset 0
				} else {
					set bitoffset $($n * 8)
				}
				continue
			}
			default {
				if {![info exists ::binary::scalarinfo($t)]} {
					return -code error "bad field specifier \"$t\""
				}
				# A scalar (integer or float) type
				lassign $::binary::scalarinfo($t) type convtype size prefix
				set var [binary::nextarg varName]

				if {$n eq "*"} {
					set n $($rembytes * 8 / $size)
				} else {
					if {$n eq ""} {
						set n 1
					}
				}
				if {$n * $size > $rembytes * 8} {
					break
				}

				if {$type in {hex bin}} {
					set u u
				}
				set convtype -$u$::binary::convtype($convtype)

				set result {}
				loop i 0 $n {
					set v [unpack $value $convtype $bitoffset $size]
					if {$type in {bin hex}} {
						append result [lindex {0 1 2 3 4 5 6 7 8 9 a b c d e f} $v]
					} else {
						lappend result $v
					}
					incr bitoffset $size
				}
				# Now skip to the end of the current byte
				if {$bitoffset % 8} {
					incr bitoffset $(8 - ($bitoffset % 8))
				}
			}
		}
		uplevel 1 [list set $var $result]
		incr count
	}
	return $count
}

# Pops the next arg from the front of the list and returns it.
# Throws an error if no more args
proc binary::nextarg {&arglist} {
	if {[llength $arglist] == 0} {
		return -level 2 -code error "not enough arguments for all format specifiers"
	}
	set arglist [lassign $arglist arg]
	return $arg
}

set binary::scalarinfo {
	c {int be 8}
	s {int le 16}
	t {int host 16}
	S {int be 16}
	i {int le 32}
	I {int be 32}
	n {int host 32}
	w {int le 64}
	W {int be 64}
	m {int host 64}
	h {hex le 4 0x}
	H {hex be 4 0x}
	b {bin le 1}
	B {bin be 1}
	r {float fle 32}
	R {float fbe 32}
	f {float fhost 32}
	q {float fle 64}
	Q {float fbe 64}
	d {float fhost 64}
}
set binary::convtype {
	be intbe
	le intle
	fbe floatbe
	fle floatle
}
if {$::tcl_platform(byteOrder) eq "bigEndian"} {
	array set binary::convtype {host intbe fhost floatbe}
} else {
	array set binary::convtype {host intle fhost floatle}
}
