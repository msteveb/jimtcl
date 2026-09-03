package require sdl
package require oo

set xres 640
set yres 384
set s [sdl.screen $xres $yres "Jim Tcl - SDL, Eventloop integration"]

set col(cyan) {0 255 255}
set col(yellow) {255 255 0}
set col(red) {255 0 0}
set col(green) {0 255 0}
set col(white) {255 255 255}
set col(blue) {0 0 255}
set ncols [dict size $col]

set grey {50 50 50}

class ball {
	name -
	pos {x 256 y 256}
	color {255 255 255}
	res {x 512 y 512}
	delta {x 3 y 3}
	radius 40
	havetext 1
}

ball method draw {s} {
	$s fcircle $pos(x) $pos(y) $radius {*}$color
	if {$havetext} {
		$s text "($pos(x),$pos(y))" $pos(x)-25 $pos(y)-5 0 0 0
	}
	foreach xy {x y} {
		incr pos($xy) $delta($xy)
		if {$pos($xy) <= $radius + $delta($xy) || $pos($xy) >= $res($xy) - $radius - $delta($xy) || [rand 50] == 1} {
			set delta($xy) $(-1 * $delta($xy))
			incr pos($xy) $(2 * $delta($xy))
		}
	}
}

ball method setvar {name_ value_} {
	set $name_ $value_
}

try {
	$s font [file dirname [info script]]/FreeSans.ttf 12
	set havetext 1
} on error msg {
	puts $msg
	set havetext 0
}

foreach c [dict keys $col] {
	set b [ball]
	$b setvar name $c
	$b setvar res(x) $xres
	$b setvar res(y) $yres
	$b setvar pos(x) $($xres/2)
	$b setvar pos(y) $($yres/2)
	$b setvar color [list {*}$col($c) 150]
	$b setvar havetext $havetext
	lappend balls $b
}

proc draw {balls} {s} {
	$s clear {*}$::grey
	foreach ball $balls {
		$ball draw $s
	}
	$s flip
}

# Example of integrating the Tcl event loop with SDL
# We need to always be polling SDL, and also run the Tcl event loop

# The Tcl event loop runs from within the SDL poll loop via
# a (non-blocking) call to update
proc heartbeat {} {
	puts $([clock millis] % 1000000)
	after 250 heartbeat
}

set t1 [clock millis]
draw $balls
heartbeat
$s poll {
	draw $balls
	update
	set t2 [clock millis]
	# 33ms = 30 frames/second
	if {$t2 - $t1 < 33} {
		after $(33 - ($t2 - $t1))
	}
	set t1 $t2
}
