# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Implements a Tcl-compatible glob command based on readdir
#
# This file is licenced under the FreeBSD license
# See LICENCE in this directory for full details.


# Implements the Tcl glob command
#
# Usage: glob ?-nocomplain? pattern ...
#
# Patterns use 'string match' (glob) pattern matching for each
# directory level, plus support for braced alternations.
#
# e.g. glob "te[a-e]*/*.{c,tcl}"
#
# Note: files starting with . will only be returned if matching component
#       of the pattern starts with .
proc glob {args} {

	# If $dir is a directory, return a list of all entries
	# it contains which match $pattern
	#
	local proc glob.readdir_pattern {dir pattern} {
		set result {}

		# readdir doesn't return . or .., so simulate it here
		if {$pattern in {. ..}} {
			return $pattern
		}

		# Use -nocomplain here to return nothing if $dir is not a directory
		foreach name [readdir -nocomplain $dir] {
			if {[string match $pattern $name]} {
				# Only include entries starting with . if the pattern starts with .
				if {[string index $name 0] eq "." && [string index $pattern 0] ne "."} {
					continue
				}
				lappend result $name
			}
		}

		return $result
	}

	# glob entries in directory $dir and pattern $rem
	#
	local proc glob.do {dir rem} {
		# Take one level from rem
		# Avoid regexp here
		set i [string first / $rem]
		if {$i < 0} {
			set pattern $rem
			set rempattern ""
		} else {
			set pattern [string range $rem 0 $i-1]
			set rempattern [string range $rem $i+1 end]
		}

		# Determine the appropriate separator and globbing dir
		set sep /
		set globdir $dir
		if {[string match "*/" $dir]} {
			set sep ""
		} elseif {$dir eq ""} {
			set globdir .
			set sep ""
		}

		set result {}

		# If the pattern contains a braced expression, recursively call glob.do
		# to expand the alternations. Avoid regexp for dependency reasons.
		# XXX: Doesn't handle backslashed braces
		if {[set fb [string first "\{" $pattern]] >= 0} {
			if {[set nb [string first "\}" $pattern $fb]] >= 0} {
				set before [string range $pattern 0 $fb-1]
				set braced [string range $pattern $fb+1 $nb-1]
				set after [string range $pattern $nb+1 end]

				foreach part [split $braced ,] {
					lappend result {*}[glob.do $dir $before$part$after]
				}
				return $result
			}
		}

		# Use readdir and select all files which match the pattern
		foreach f [glob.readdir_pattern $globdir $pattern] {
			if {$rempattern eq ""} {
				# This is a terminal entry, so add it
				lappend result $dir$sep$f
			} else {
				# Expany any entries at this level and add them
				lappend result {*}[glob.do $dir$sep$f $rempattern]
			}
		}
		return $result
	}

	# Start of main glob
	set nocomplain 0

	if {[lindex $args 0] eq "-nocomplain"} {
		set nocomplain 1
		set args [lrange $args 1 end]
	}

	set result {}
	foreach pattern $args {
		if {$pattern eq "/"} {
			lappend result /
		} elseif {[string match "/*" $pattern]} {
			lappend result {*}[glob.do / [string range $pattern 1 end]]
		} else {
			lappend result {*}[glob.do "" $pattern]
		}
	}

	if {$nocomplain == 0 && [llength $result] == 0} {
		return -code error "no files matched glob patterns"
	}

	return $result
}
