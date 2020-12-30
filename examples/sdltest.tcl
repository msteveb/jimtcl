package require sdl

# Basic test of all sdl commands

set xres 640
set yres 384
set s [sdl.screen $xres $yres]

set cyan {0 255 255 200}
set yellow {255 255 0 200}
set red {255 0 0 200}
set green {0 255 0 200}
set grey {50 50 50 200}
set white {255 255 255}
set blue {0 0 255 200}

$s clear {*}$grey

$s fcircle 320 280 40 {*}$cyan
$s circle 320 280 60 {*}$yellow
$s aacircle 320 280 80 {*}$green

$s rectangle 200 100 300 180 {*}$cyan
$s box 210 110 290 170 {*}$yellow

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

$s rectangle 50 200 150 300 {*}$yellow
foreach i [range 500] {
	$s pixel $([rand 100] + 50) $([rand 100] + 200) {*}$white
}

$s poll { sleep 0.25 }
$s free
