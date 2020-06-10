#!/usr/bin/env tclsh
#
# Tabulate the output of Jim's bench.tcl -batch
#
# Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
#

proc changed_amount {firstvalue v} {
    set x [expr {($v + 0.0) / $firstvalue}]
    if {$x >= 0.99 && $x <= 1.01} {
        set val .
    } elseif {$v > $firstvalue} {
        set p [expr {(($v - $firstvalue) * 100.0) / $firstvalue}]
        set val [format "%+3.0f" $p]%
    } else {
        set p [expr {(($firstvalue - $v) * 100.0) / $firstvalue}]
        set val [format "%+3.0f" -$p]%
    }
    return $val
}

proc format_us_time {us} {
    set units {2 ps 1 ns 0 us -1 ms -2 s}

    if {$us >= 1e8} {
            # >= 100 seconds
            return [format "%.0fs" [expr {$us / 1e6}]]
    }

    # How many digits to the left of the decimal place
    set leftdigits [expr {int(floor(log10($us)) + 1)}]
    # Work out how much to shift by, in increments of 10^3
    set shift3 [expr {(-$leftdigits / 3) + 1}]
    set shift [expr {$shift3 * 3}]
    # Always show 3 significant digits
    set decimals [expr {3 - ($leftdigits + $shift)}]
    set name [dict get $units $shift3]
    set value $($us * pow(10.0,$shift))
    return [format "%.${decimals}f%s" $value $name]
}

proc main {filename} {
    set versions {}
    array set bench {}
    set f [open $filename r]
    while {[gets $f data] >= 0} {
        lappend versions [lindex $data 0]
        set results [lindex $data 1]
        foreach {title time} $results {
            lappend bench($title) $time
        }
    }
    close $f

    puts "Jim benchmarks - time in milliseconds"
    puts -nonewline [string repeat " " 21]
    foreach v $versions {
        puts -nonewline [format "% 6s " $v]
    }
    puts ""

    foreach test [lsort [array names bench]] {
        set col 0
        puts -nonewline "[format {% 20s} $test] "
        foreach v $bench($test) {
            if {$v eq "F"} {
                set val "F"
            } else {
                if {$col == 0} {
                    set val [format_us_time $v]
                } else {
                    set val [changed_amount [lindex $bench($test) 0] $v]
                }
            }
            puts -nonewline [format "%6s " $val]
            incr col
        }
        puts ""
    }
}

if {!$tcl_interactive} {
    set r [catch {eval [linsert $argv 0 main]} res]
    puts $res
    exit $r
}
