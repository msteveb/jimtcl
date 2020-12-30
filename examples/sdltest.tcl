package require sdl

set xres 1024
set yres 768
set s [sdl.screen $xres $yres]

proc drawlist {s list} {
    foreach item $list {
        $s {*}$item
    }
}

proc rand_circle {xres yres maxradius alpha} {
    list fcircle [rand $xres] [rand $yres] [rand $maxradius] [rand 256] [rand 256] [rand 256] $alpha
}

loop i 0 200 {
    set commands {}
    loop j 0 1000 {
        lappend commands [rand_circle $xres $yres 40 100]
        if {$j % 50 == 0} {
            #$s clear 200 200 200
            drawlist $s $commands
            $s flip
            sleep 0.1
        }
    }
}
