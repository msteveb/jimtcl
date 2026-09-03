package require sdl

# Basic test of all sdl commands

set xres 640
set yres 384
set s [sdl.screen $xres $yres [info script]]

set cyan {0 255 255}
set yellow {255 255 0}
set red {255 0 0}
set green {0 255 0}
set grey {20 20 20}
set white {255 255 255}
set blue {0 0 255}

$s clear {*}$grey

$s fcircle 320 280 40 {*}$cyan 150
$s circle 320 280 60 {*}$yellow
$s aacircle 320 280 80 {*}$green

$s rectangle 200 100 300 180 {*}$cyan
$s box 210 110 290 170 {*}$yellow 150

set x 20
set y 20
set dy 10
set dx 10
foreach i [range 50] {
	set nx $($x + $dx)
	set ny $($y + $dy)
	$s line $x $y $nx $ny {*}$green
	$s aaline $x $($y+30) $nx $($ny+30) {*}$red
	set x $nx
	set y $ny
	set dy $(-$dy)
}

$s rectangle 50 150 150 250 {*}$yellow
foreach i [range 500] {
	$s pixel $([rand 100] + 50) $([rand 100] + 150) {*}$white
}

if {[llength $argv]} {
	lassign $argv font
} else {
	set font [file join [file dirname [info script]] FreeSans.ttf]
}

try {
	$s font $font 18
	$s text "[file tail $font] 16pt" 20 270 {*}$yellow
	$s font $font 14
	$s text "[file tail $font] 12pt" 20 300 {*}$green 150
	# Note that depending on the font, certain unicode glyphs
	# may or may not be rendered.
	# Also, need to build with --utf8
	$s text "utf-8: \u00bb \u273b \u261e" 20 330 {*}$cyan
} on error msg {
	puts $msg
}

$s poll { sleep 0.25 }
$s free
