set batchmode 0
set benchmarks {}
# Run each benchmark for this long (ms)
# Can be changed with the -time parameter
set benchtime 1000

# If the timerate command doesn't exist, implement it in Tcl
if {[info commands timerate] eq ""} {
    proc timerate {script {ms 1000}} {
        set start [clock micros]
        set stop [expr {$start + $ms * 1000}]
        set count 0
        while {1} {
            uplevel 1 $script
            incr count
            set now [clock micros]
            if {$now >= $stop} {
                break
            }
        }
        set elapsed [expr {$now - $start}]

        # Now try to account for the Tcl overhead
        set start [clock micros]
        set n 0
        while {1} {
            uplevel 1 {}
            incr n
            set now [clock micros]
            if {$n >= $count} {
                break
            }
        }
        set elapsed [expr {$elapsed - ($now - $start)}]

        list us_per_iter [expr {1.0 * $elapsed / $count}] iters_per_sec [expr {1e6 * $count / $elapsed}] \
            count $count elapsed_us $elapsed
    }
}

proc format_us_time {us} {
    set units {2 {ps 1e6} 1 {ns 1e3} 0 {us 1} -1 {ms 1e-3} -2 {s 1e-6}}

    if {$us >= 1e8} {
            # >= 100 seconds
            return [format "%.0fs" [expr {$us / 1e6}]]
    }

    # Avoid using log10 here in case math functions aren't enabled

    # How many digits to the left of the decimal place?
    lassign [split [format %e $us] e] - exp
    # Work around Tcl's stupid auto-octal detection
    set exp [regsub -all {([-+])(0+)?(\d)} $exp {\1\3}]
    set leftdigits [expr {$exp + 1}]
    #set leftdigits [expr {int(floor(log10($us)) + 1)}]
    #puts "$leftdigits1 $leftdigits"
    # Work out how much to shift by, in increments of 10^3
    set shift3 [expr {(-$leftdigits / 3) + 1}]
    set shift [expr {$shift3 * 3}]
    # Always show 3 significant digits
    set decimals [expr {3 - ($leftdigits + $shift)}]
    lassign [dict get $units $shift3] name mult
    set value [expr {$us * $mult}]
    return [format "%.${decimals}f%s" $value $name]
}

proc bench {title script} {
    global benchmarks batchmode

    set Title [string range "$title                     " 0 20]

    catch {collect}
    set failed [catch {timerate $script $::benchtime} res]
    if {$failed} {
        if {!$batchmode} {puts "$Title - This test can't run on this interpreter ($res)"}
        lappend benchmarks $title F
    } else {
        set us [dict get $res us_per_iter]
        set count [dict get $res count]
        set ms [expr {$us / 1000}]
        lappend benchmarks $title $ms
        if {!$batchmode} { puts "$Title - [format_us_time $us] per iteration" }
    }
    catch { collect }
}

### BUSY LOOP ##################################################################

proc whilebusyloop {n} {
    set i 0
    while {$i < $n} {
        set a $i
        incr i
    }
}

proc forbusyloop {n} {
    for {set i 0} {$i < $n} {incr i} {
        set a $i
    }
}

proc loopbusyloop {n} {
    loop i 0 $n {
        set a $i
    }
}

### FIBONACCI ##################################################################

proc fibonacci {x} {
    if {$x <= 1} {
	expr 1
    } else {
	expr {[fibonacci [expr {$x-1}]] + [fibonacci [expr {$x-2}]]}
    }
}

### HEAPSORT ###################################################################

set IM 139968
set IA   3877
set IC  29573

set last 42

proc make_gen_random {} {
    global IM IA IC
    set params [list IM $IM IA $IA IC $IC]
    set body [string map $params {
        global last
        expr {($max * [set last [expr {($last * IA + IC) % IM}]]) / IM}
    }]
    proc gen_random {max} $body
}

proc heapsort {ra_name} {
    upvar 1 $ra_name ra
    set n [llength $ra]
    set l [expr {$n / 2}]
    set ir [expr {$n - 1}]
    while 1 {
        if {$l} {
            set rra [lindex $ra [incr l -1]]
        } else {
	    set rra [lindex $ra $ir]
	    lset ra $ir [lindex $ra 0]
	    if {[incr ir -1] == 0} {
                lset ra 0 $rra
		break
            }
        }
	set i $l
	set j [expr {(2 * $l) + 1}]
        while {$j <= $ir} {
	    set tmp [lindex $ra $j]
	    if {$j < $ir} {
		if {$tmp < [lindex $ra [expr {$j + 1}]]} {
		    set tmp [lindex $ra [incr j]]
		}
	    }
            if {$rra >= $tmp} {
		break
	    }
	    lset ra $i $tmp
	    incr j [set i $j]
        }
        lset ra $i $rra
    }
}

proc heapsort_main {n} {
    make_gen_random

    set data {}
    for {set i 1} {$i <= $n} {incr i} {
	lappend data [gen_random 1.0]
    }
    heapsort data
}

### SIEVE ######################################################################

proc sieve {num} {
    while {$num > 0} {
	incr num -1
	set count 0
	for {set i 2} {$i <= 8192} {incr i} {
	    set flags($i) 1
	}
	for {set i 2} {$i <= 8192} {incr i} {
	    if {$flags($i) == 1} {
		# remove all multiples of prime: i
		for {set k [expr {$i+$i}]} {$k <= 8192} {incr k $i} {
		    set flags($k) 0
		}
		incr count
	    }
	}
    }
    return $count
}

proc sieve_dict {num} {
    while {$num > 0} {
	incr num -1
	set count 0
	for {set i 2} {$i <= 8192} {incr i} {
	    dict set flags $i 1
	}
	for {set i 2} {$i <= 8192} {incr i} {
	    if {[dict get $flags $i] == 1} {
		# remove all multiples of prime: i
		for {set k [expr {$i+$i}]} {$k <= 8192} {incr k $i} {
		    dict set flags $k 0
		}
		incr count
	    }
	}
    }
    return $count
}

### ARY ########################################################################

proc ary n {
    for {set i 0} {$i < $n} {incr i} {
	set x($i) $i
    }
    set last [expr {$n - 1}]
    for {set j $last} {$j >= 0} {incr j -1} {
	set y($j) $x($j)
    }
}

proc ary_dict n {
    for {set i 0} {$i < $n} {incr i} {
	dict set x $i $i
    }
    set last [expr {$n - 1}]
    for {set j $last} {$j >= 0} {incr j -1} {
	dict set y $j $x($j)
    }
}

proc ary_static n {
    for {set i 0} {$i < $n} {incr i} {
        set a(b) $i
        set a(c) $i
    }
}

### REPEAT #####################################################################

proc repeat {n body} {
    for {set i 0} {$i < $n} {incr i} {
	uplevel 1 $body
    }
}

proc use_repeat {n} {
    set x 0
    repeat $n {incr x}
}

### UPVAR ######################################################################

proc myincr varname {
    upvar 1 $varname x
    incr x
}

proc upvartest {n} {
    set y 0
    for {set x 0} {$x < $n} {myincr x} {
	myincr y
    }
}

### NESTED LOOPS ###############################################################

proc nestedloops {n} {
    set x 0
    incr n 1
    set a $n
    while {[incr a -1]} {
	set b $n
	while {[incr b -1]} {
	    set c $n
	    while {[incr c -1]} {
		set d $n
		while {[incr d -1]} {
		    set e $n
		    while {[incr e -1]} {
			set f $n
			while {[incr f -1]} {
			    incr x
			}
		    }
		}
	    }
	}
    }
}

### ROTATE #####################################################################

proc rotate {count} {
    set v 1
    for {set n 0} {$n < $count} {incr n} {
	set v [expr {$v <<< 1}]
    }
}

### DYNAMICALLY GENERATED CODE #################################################

proc dyncode {n} {
    for {set i 0} {$i < $n} {incr i} {
        set script "lappend foo $i"
        eval $script
    }
}

proc dyncode_list {n} {
    for {set i 0} {$i < $n} {incr i} {
        set script [list lappend foo $i]
        eval $script
    }
}

### LIST #################################################

proc listcreate {n} {
    for {set i 0} {$i < $n} {incr i} {
        set a [list a b c d e f]
    }
}

### PI DIGITS ##################################################################

proc pi_digits {N} {
 set n [expr {$N * 3}]
 set e 0
 set f {}
 for { set b 0 } { $b <= $n } { incr b } {
     lappend f 2000
 }
 for { set c $n } { $c > 0 } { incr c -14 } {
     set d 0
     set g [expr { $c * 2 }]
     set b $c
     while 1 {
         incr d [expr { [lindex $f $b] * 10000 }]
         lset f $b [expr {$d % [incr g -1]}]
         set d [expr { $d / $g }]
         incr g -1
         if { [incr b -1] == 0 } break
         set d [expr { $d * $b }]
     }
     append result [string range 0000[expr { $e + $d / 10000 }] end-3 end]
     set e [expr { $d % 10000 }]
 }
 #puts $result
}

### EXPAND #####################################################################

proc expand {} {
    set a [list a b c d e f]
    for {set i 0} {$i < 100000} {incr i} {
        lappend b {*}$a
    }
}

### MINLOOPS ###################################################################

proc miniloops {n} {
    for {set i 0} {$i < $n} {incr i} {
        set sum 0
        for {set j 0} {$j < 10} {incr j} {
            # something more or less real
            incr sum $j
        }
    }
}

### wiki.tcl.tk/8566 ###########################################################

 # Internal procedure that indexes into the 2-dimensional array t,
 # which corresponds to the sequence y, looking for the (i,j)th element.

 proc Index { t y i j } {
     set indx [expr { ([llength $y] + 1) * ($i + 1) + ($j + 1) }]
     return [lindex $t $indx]
 }

 # Internal procedure that implements Levenshtein to derive the longest
 # common subsequence of two lists x and y.

 proc ComputeLCS { x y } {
     set t [list]
     for { set i -1 } { $i < [llength $y] } { incr i } {
         lappend t 0
     }
     for { set i 0 } { $i < [llength $x] } { incr i } {
         lappend t 0
         for { set j 0 } { $j < [llength $y] } { incr j } {
             if { [string equal [lindex $x $i] [lindex $y $j]] } {
                 set lastT [Index $t $y [expr { $i - 1 }] [expr {$j - 1}]]
                 set nextT [expr {$lastT + 1}]
             } else {
                 set lastT1 [Index $t $y $i [expr { $j - 1 }]]
                 set lastT2 [Index $t $y [expr { $i - 1 }] $j]
                 if { $lastT1 > $lastT2 } {
                     set nextT $lastT1
                 } else {
                     set nextT $lastT2
                 }
             }
             lappend t $nextT
         }
     }
     return $t
 }

 # Internal procedure that traces through the array built by ComputeLCS
 # and finds a longest common subsequence -- specifically, the one that
 # is lexicographically first.

 proc TraceLCS { t x y } {
     set trace {}
     set i [expr { [llength $x] - 1 }]
     set j [expr { [llength $y] - 1 }]
     set k [expr { [Index $t $y $i $j] - 1 }]
     while { $i >= 0 && $j >= 0 } {
         set im1 [expr { $i - 1 }]
         set jm1 [expr { $j - 1 }]
         if { [Index $t $y $i $j] == [Index $t $y $im1 $jm1] + 1
              && [string equal [lindex $x $i] [lindex $y $j]] } {
             lappend trace xy [list $i $j]
             set i $im1
             set j $jm1
         } elseif { [Index $t $y $im1 $j] > [Index $t $y $i $jm1] } {
             lappend trace x $i
             set i $im1
         } else {
             lappend trace y $j
             set j $jm1
         }
     }
     while { $i >= 0 } {
         lappend trace x $i
         incr i -1
     }
     while { $j >= 0 } {
         lappend trace y $j
         incr j -1
     }
     return $trace
 }

 # list::longestCommonSubsequence::compare --
 #
 #       Compare two lists for the longest common subsequence
 #
 # Arguments:
 #       x, y - Two lists of strings to compare
 #       matched - Callback to execute on matched elements, see below
 #       unmatchedX - Callback to execute on unmatched elements from the
 #                    first list, see below.
 #       unmatchedY - Callback to execute on unmatched elements from the
 #                    second list, see below.
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       Whatever the callbacks do.
 #
 # The 'compare' procedure compares the two lists of strings, x and y.
 # It finds a longest common subsequence between the two.  It then walks
 # the lists in order and makes the following callbacks:
 #
 # For an element that is common to both lists, it appends the index in
 # the first list, the index in the second list, and the string value of
 # the element as three parameters to the 'matched' callback, and executes
 # the result.
 #
 # For an element that is in the first list but not the second, it appends
 # the index in the first list and the string value of the element as two
 # parameters to the 'unmatchedX' callback and executes the result.
 #
 # For an element that is in the second list but not the first, it appends
 # the index in the second list and the string value of the element as two
 # parameters to the 'unmatchedY' callback and executes the result.

 proc compare { x y
                                                matched
                                                unmatchedX unmatchedY } {
     set t [ComputeLCS $x $y]
     set trace [TraceLCS $t $x $y]
     set i [llength $trace]
     while { $i > 0 } {
         set indices [lindex $trace [incr i -1]]
         set type [lindex $trace [incr i -1]]
         switch -exact -- $type {
             xy {
                 set c $matched
                 eval lappend c $indices
                 lappend c [lindex $x [lindex $indices 0]]
                 uplevel 1 $c
             }
             x {
                 set c $unmatchedX
                 lappend c $indices
                 lappend c [lindex $x $indices]
                 uplevel 1 $c
             }
             y {
                 set c $unmatchedY
                 lappend c $indices
                 lappend c [lindex $y $indices]
                 uplevel 1 $c
             }
         }
     }
     return
 }

 proc umx { index value } {
     global lastx
     global xlines
     append xlines "< " $value \n
     set lastx $index
 }

 proc umy { index value } {
     global lasty
     global ylines
     append ylines "> " $value \n
     set lasty $index
 }

 proc matched { index1 index2 value } {
     global lastx
     global lasty
     global xlines
     global ylines
     if { [info exists lastx] && [info exists lasty] } {
     #puts "[expr { $lastx + 1 }],${index1}c[expr {$lasty + 1 }],${index2}"
     #puts -nonewline $xlines
     #puts "----"
     #puts -nonewline $ylines
     } elseif { [info exists lastx] } {
     #puts "[expr { $lastx + 1 }],${index1}d${index2}"
     #puts -nonewline $xlines
     } elseif { [info exists lasty] } {
     #puts  "${index1}a[expr {$lasty + 1 }],${index2}"
     #puts -nonewline $ylines
     }
     catch { unset lastx }
     catch { unset xlines }
     catch { unset lasty }
     catch { unset ylines }
 }

 # Really, we should read the first file in like this:
 #    set f0 [open [lindex $argv 0] r]
 #    set x [split [read $f0] \n]
 #    close $f0
 # But I'll just provide some sample lines:

proc commonsub_test {n} {
 set x {}
 for { set i 0 } { $i < $n } { incr i } {
     lappend x a r a d e d a b r a x
 }

 # The second file, too, should be read in like this:
 #    set f1 [open [lindex $argv 1] r]
 #    set y [split [read $f1] \n]
 #    close $f1
 # Once again, I'll just do some sample lines.

 set y {}
 for { set i 0 } { $i < $n } { incr i } {
     lappend y a b r a c a d a b r a
 }

 compare $x $y matched umx umy
 matched [llength $x] [llength $y] {}
}

### MANDEL #####################################################################

proc mandel {xres yres infx infy supx supy} {
    set incremx [expr {(0.0+$supx-$infx)/$xres}]
    set incremy [expr {(0.0+$supy-$infy)/$yres}]

    for {set j 0} {$j < $yres} {incr j} {
	set cim [expr {$infy+$incremy*$j}]
	set line {}
	for {set i 0} {$i < $xres} {incr i} {
	    set counter 0
	    set zim 0
	    set zre 0
	    set cre [expr {$infx+$incremx*$i}]
	    while {$counter < 255} {
		set dam [expr {$zre*$zre-$zim*$zim+$cre}]
		set zim [expr {2*$zim*$zre+$cim}]
		set zre $dam
		if {$zre*$zre+$zim*$zim > 4} break
		incr counter
	    }
	    # output pixel $i $j
	}
    }
}

### RUN ALL ####################################################################

# bench.tcl ?-batch? ?-time <ms>? ?version?

while [llength $argv] {
    switch -glob -- [lindex $argv 0] {
        -batch {
            set batchmode 1
            set argv [lrange $argv 1 end]
        }
        -time {
            set arg [lindex $argv 1]
            if {$arg ne ""} {
                set benchtime $arg
            }
            set argv [lrange $argv 2 end]
        }
        default {
            break
        }
    }
}
set ver [lindex $argv 0]

bench {[while] busy loop} {whilebusyloop 10}
bench {[for] busy loop} {forbusyloop 10}
bench {[loop] busy loop} {loopbusyloop 10}
bench {mini loops} {miniloops 10}
bench {fibonacci(4)} {fibonacci 4}
bench {heapsort} {heapsort_main 50}
bench {sieve} {sieve 1}
bench {sieve [dict]} {sieve_dict 1}
bench {ary} {ary 20}
bench {ary [dict]} {ary_dict 20}
bench {ary [static]} {ary_static 20}
bench {repeat} {use_repeat 20}
bench {upvar} {upvartest 20}
bench {nested loops} {nestedloops 2}
bench {rotate} {rotate 100}
bench {dynamic code} {dyncode 100}
bench {dynamic code (list)} {dyncode_list 100}
bench {PI digits} {pi_digits 100}
bench {listcreate} {listcreate 100}
bench {expand} {expand}
bench {wiki.tcl.tk/8566} {commonsub_test 10}
bench {mandel} {mandel 30 30 -2 -1.5 1 1.5}

if {$batchmode} {
    if {$ver == ""} {
        if {[catch {info patchlevel} ver]} {
            set ver Jim[info version]
        }
    }
    puts [list $ver $benchmarks]
}
