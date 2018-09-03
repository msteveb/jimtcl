# Minimal support for package require
# No error on failure since C extensions aren't handled
proc package {cmd pkg args} {
	if {$cmd eq "require"} {
		foreach path $::auto_path {
			set pkgpath $path/$pkg.tcl
			if {$path eq "."} {
				set pkgpath $pkg.tcl
			}
			if {[file exists $pkgpath]} {
				uplevel #0 [list source $pkgpath]
				return
			}
		}
	}
}
