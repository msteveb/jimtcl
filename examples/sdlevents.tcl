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
}

ball method draw {s} {
	$s fcircle $pos(x) $pos(y) $radius {*}$color
	foreach xy {x y} {
		incr pos($xy) $delta($xy)
		if {$pos($xy) <= $radius + $delta($xy) || $pos($xy) >= $res($xy) - $radius - $delta($xy) || [rand 50] == 1} {
			set delta($xy) $(-1 * $delta($xy))
			incr pos($xy) $(2 * $delta($xy))
		}
	}
}

foreach c [dict keys $col] {
	set b [ball]
	$b eval [list set name $c]
	$b eval [list set res(x) $xres]
	$b eval [list set res(y) $yres]
	$b eval [list set pos(x) $($xres/2)]
	$b eval [list set pos(y) $($yres/2)]
	$b eval [list set color [list {*}$col($c) 150]]
	lappend balls $b
}

proc draw {balls} {s} {
	global x y dx dy xres yres
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

draw $balls
heartbeat
$s poll {
	# 16ms = 60 frames/second
	# Could take into account the drawing time
	after 16
	draw $balls
	update
}
