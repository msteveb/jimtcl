set batchmode 0
set benchmarks {}

proc bench {title script} {
    global benchmarks batchmode

    set Title [string range "$title                     " 0 20]

    set failed [catch {time $script} res]
    if {$failed} {
        if {!$batchmode} {puts "$Title - This test can't run on this interpreter"}
        lappend benchmarks $title F
    } else {
        set t [expr [lindex $res 0] / 1000]
        lappend benchmarks $title $t
        set ts "          $t"
        set ts [string range $ts [expr {[string length $ts]-10}] end]
        if {!$batchmode} {puts "$Title -$ts ms per iteration"}
    }
}

### BUSY LOOP ##################################################################

proc whilebusyloop {} {
    set i 0
    while {$i < 1850000} {
	incr i
    }
}

proc forbusyloop {} {
    for {set i 0} {$i < 1850000} {incr i} {}
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

proc heapsort_main {} {
    set n 6100
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

### REPEAT #####################################################################

proc repeat {n body} {
    for {set i 0} {$i < $n} {incr i} {
	uplevel 1 $body
    }
}

proc use_repeat {} {
    set x 0
    repeat {1000000} {incr x}
}

### UPVAR ######################################################################

proc myincr varname {
    upvar 1 $varname x
    incr x
}

proc upvartest {} {
    set y 0
    for {set x 0} {$x < 100000} {myincr x} {
	myincr y
    }
}

### NESTED LOOPS ###############################################################

proc nestedloops {} {
    set n 10
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

proc dyncode {} {
    for {set i 0} {$i < 100000} {incr i} {
        set script "lappend foo $i"
        eval $script
    }
}

proc dyncode_list {} {
    for {set i 0} {$i < 100000} {incr i} {
        set script [list lappend foo $i]
        eval $script
    }
}

### PI DIGITS ##################################################################

proc pi_digits {} {
    set N 300
    set LEN [expr {10*$N/3}]
    set result ""

    set a [string repeat " 2" $LEN]
    set nines 0
    set predigit 0
    set nines {}

    set i0 [expr {$LEN+1}]
    set quot0 [expr {2*$LEN+1}]
    for {set j 0} {$j<$N} {incr j} {
        set q 0
        set i $i0
        set quot $quot0
        set pos -1
        foreach apos $a {
            set x [expr {10*$apos + $q * [incr i -1]}]
            lset a [incr pos] [expr {$x % [incr quot -2]}]
            set q [expr {$x / $quot}]
        }
        lset a end [expr {$q % 10}]
        set q [expr {$q / 10}]
        if {$q < 8} {
            append result $predigit $nines
            set nines {}
            set predigit $q
        } elseif {$q == 9} {
            append nines 9
        } else {
            append result [expr {$predigit+1}][string map {9 0} $nines]
            set nines {}
            set predigit 0
        }
    }
    #puts $result$predigit
}

### EXPAND #####################################################################

proc expand {} {
    for {set i 0} {$i < 100000} {incr i} {
        set a [list a b c d e f]
        lappend b {expand}$a
    }
}

### MINLOOPS ###################################################################

proc miniloops {} {
    for {set i 0} {$i < 100000} {incr i} {
        set sum 0
        for {set j 0} {$j < 10} {incr j} {
            # something of more or less real
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

proc commonsub_test {} {
 set x {}
 for { set i 0 } { $i < 20 } { incr i } {
     lappend x a r a d e d a b r a x
 }

 # The second file, too, should be read in like this:
 #    set f1 [open [lindex $argv 1] r]
 #    set y [split [read $f1] \n]
 #    close $f1
 # Once again, I'll just do some sample lines.

 set y {}
 for { set i 0 } { $i < 20 } { incr i } {
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
	set cim [expr {$infy+($incremy*$j)}]
	set line {}
	for {set i 0} {$i < $xres} {incr i} {
	    set counter 0
	    set zim 0
	    set zre 0
	    set cre [expr {$infx+($incremx*$i)}]
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

if {[string compare [lindex $argv 0] "-batch"] == 0} {
    set batchmode 1
}

bench {[while] busy loop} {whilebusyloop}
bench {[for] busy loop} {forbusyloop}
bench {mini loops} {miniloops}
bench {fibonacci(25)} {fibonacci 25}
bench {heapsort} {heapsort_main}
bench {sieve} {sieve 10}
bench {sieve [dict]} {sieve_dict 10}
bench {ary} {ary 100000}
bench {repeat} {use_repeat}
bench {upvar} {upvartest}
bench {nested loops} {nestedloops}
bench {rotate} {rotate 100000}
bench {dynamic code} {dyncode}
bench {dynamic code (list)} {dyncode_list}
bench {PI digits} {pi_digits}
bench {expand} {expand}
bench {wiki.tcl.tk/8566} {commonsub_test}
bench {mandel} {mandel 60 60 -2 -1.5 1 1.5}

proc istcl {} {
    return [expr {![catch {info tclversion}]}]
}

if {$batchmode} {
    if {[catch {info patchlevel} ver]} {
        set ver Jim[info version]
    }
    puts [list $ver $benchmarks]
}
