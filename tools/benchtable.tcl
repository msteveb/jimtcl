#!/usr/bin/env tclsh
#
# Tabulate the output of Jim's bench.tcl -batch
#
# Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
#

proc main {filename} {
    set versions {}
    array set bench {}
    set f [open $filename r]
    while {![eof $f]} {
        gets $f data
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

    foreach test [array names bench] {
        puts -nonewline "[format {% 20s} $test] "
        foreach v $bench($test) {
            if {$v eq "F"} {
                puts -nonewline "     F "
            } else {
                puts -nonewline [format "% 6d " $v]
            }
        }
        puts ""
    }
}

if {!$tcl_interactive} {
    set r [catch {eval [linsert $argv 0 main]} res]
    puts $res
    exit $r
}
