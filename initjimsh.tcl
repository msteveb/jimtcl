# This pseudo-package is loaded from jimsh to add additional
# paths to $auto_path and to source ~/.jimrc

proc _jimsh_init {} {
	rename _jimsh_init {}
	global jim::exe jim::argv0 tcl_interactive auto_path tcl_platform

	# Stash the result of [info nameofexecutable] now, before a possible [cd]
	if {[string match "*/*" $jim::argv0]} {
		set jim::exe [file join [pwd] $jim::argv0]
	} else {
		set jim::exe ""
		foreach path [split [env PATH ""] $tcl_platform(pathSeparator)] {
			set exec [file join [pwd] [string map {\\ /} $path] $jim::argv0]
			if {[file executable $exec]} {
				set jim::exe $exec
				break
			}
		}
	}

	# Add to the standard auto_path
	lappend p {*}[split [env JIMLIB {}] $tcl_platform(pathSeparator)]
	lappend p [file dirname $jim::exe]
	lappend p {*}$auto_path
	set auto_path $p

	if {$tcl_interactive && [env HOME {}] ne ""} {
		foreach src {.jimrc jimrc.tcl} {
			if {[file exists [env HOME]/$src]} {
				uplevel #0 source [env HOME]/$src
				break
			}
		}
	}
	return ""
}

if {$tcl_platform(platform) eq "windows"} {
	set jim::argv0 [string map {\\ /} $jim::argv0]
}

_jimsh_init
