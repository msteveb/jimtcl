# Minimal support for package require
# No error on failure since C extensions aren't handled
proc package {cmd pkg} {
	if {$cmd eq "require"} {
		foreach path $::auto_path {
			if {[file exists $path/$pkg.tcl]} {
				uplevel #0 [list source $path/$pkg.tcl]
				return
			}
		}
	}
}
