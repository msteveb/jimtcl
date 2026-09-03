# Minimal support for package require
proc package {cmd args} {
	if {$cmd eq "require"} {
		foreach path $::auto_path {
			lassign $args pkg
			set pkgpath $path/$pkg.tcl
			if {$path eq "."} {
				set pkgpath $pkg.tcl
			}
			if {[file exists $pkgpath]} {
				tailcall uplevel #0 [list source $pkgpath]
			}
		}
	}
}
set tcl_platform(bootstrap) 1
