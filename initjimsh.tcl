# This pseudo-package is loaded from jimsh to add additional
# paths to $auto_path and to source ~/.jimrc

proc _jimsh_init {} {
	rename _jimsh_init {}
	global jim::exe jim::argv0 tcl_interactive auto_path tcl_platform

	# Stash the result of [info nameofexecutable] now, before a possible [cd]
	if {[exists jim::argv0]} {
		if {[string match "*/*" $jim::argv0]} {
			set jim::exe [file join [pwd] $jim::argv0]
		} else {
			set jim::argv0 [file tail $jim::argv0]
			set path [split [env PATH ""] $tcl_platform(pathSeparator)]
			if {$tcl_platform(platform) eq "windows"} {
				# Windows searches the current directory first, and convert backslashes to slashes
				set path [lmap p [list "" {*}$path] { string map {\\ /} $p }]
			}
			foreach p $path {
				set exec [file join [pwd] $p $jim::argv0]
				if {[file executable $exec]} {
					set jim::exe $exec
					break
				}
			}
		}
	}

	# Add to the standard auto_path
	lappend p {*}[split [env JIMLIB {}] $tcl_platform(pathSeparator)]
	if {[exists jim::exe]} {
		lappend p [file dirname $jim::exe]
	}
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

# Set a global variable here so that custom commands can be added post hoc
set tcl::autocomplete_commands {array clock debug dict file history info namespace package signal socket string tcl::prefix zlib}

# Simple interactive command line completion callback
# Explicitly knows about some commands that support "-commands"
proc tcl::autocomplete {prefix} {
	if {[set space [string first " " $prefix]] != -1} {
		set cmd [string range $prefix 0 $space-1]
		if {$cmd in $::tcl::autocomplete_commands || [info channel $cmd] ne ""} {
			set arg [string range $prefix $space+1 end]
			# Add any results from -commands
			return [lmap p [$cmd -commands] {
				if {![string match "${arg}*" $p]} continue
				function "$cmd $p"
			}]
		}
	}
	# Find matching files.
	if {[string match "source *" $prefix]} {
		set path [string range $prefix 7 end]
		return [lmap p [glob -nocomplain "${path}*"] {
			function "source $p"
		}]
	}
	# Find matching commands, omitting results containing spaces
	return [lmap p [lsort [info commands $prefix*]] {
		if {[string match "* *" $p]} {
			continue
		}
		function $p
	}]
}

# Only procs and C commands that support "cmd -help subcommand" have autohint suport
set tcl::stdhint_commands {array clock debug dict file history info namespace package signal string zlib}

set tcl::stdhint_cols {
	none {0}
	black {30}
	red {31}
	green {32}
	yellow {33}
	blue {34}
	purple {35}
	cyan {36}
	normal {37}
	grey {30 1}
	gray {30 1}
	lred {31 1}
	lgreen {32 1}
	lyellow {33 1}
	lblue {34 1}
	lpurple {35 1}
	lcyan {36 1}
	white {37 1}
}

# Make it easy to change the colour
set tcl::stdhint_col $tcl::stdhint_cols(lcyan)

# The default hint implementation
proc tcl::stdhint {string} {
	set result ""
	if {[llength $string] >= 2} {
		lassign $string cmd arg
		if {$cmd in $::tcl::stdhint_commands || [info channel $cmd] ne ""} {
			catch {
				set help [$cmd -help $arg]
				if {[string match "Usage: $cmd *" $help]} {
					set n [llength $string]
					set subcmd [lindex $help $n]
					incr n
					set hint [join [lrange $help $n end]]
					set prefix ""
					if {![string match "* " $string]} {
						if {$n == 3 && $subcmd ne $arg} {
							# complete the subcommand in the hint
							set prefix "[string range $subcmd [string length $arg] end] "
						} else {
							set prefix " "
						}
					}
					set result [list $prefix$hint {*}$::tcl::stdhint_col]
				}
			}
		}
	}
	return $result
}

_jimsh_init
