# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Loads some Tcl-compatible features.
# case, lassign, parray, errorInfo, ::tcl_platform, ::env

package provide tclcompat 1.0

# Set up the ::env array
set env [env]

# Tcl 8.5 lassign
proc lassign {list args} {
	uplevel 1 [list foreach $args [concat $list {}] break]
	lrange $list [llength $args] end
}

# case var ?in? pattern action ?pattern action ...?
proc case {var args} {
	# Skip dummy parameter
	if {[lindex $args 0] eq "in"} {
		set args [lrange $args 1 end]
	}

	# Check for single arg form
	if {[llength $args] == 1} {
		set args [lindex $args 0]
	}

	# Check for odd number of args
	if {[llength $args] % 2 != 0} {
		error "extra case pattern with no body"
	}

	# Internal function to match a value agains a list of patterns
	set checker [lambda {value pattern} {string match $pattern $value}]

	foreach {value action} $args {
		if {$value eq "default"} {
			set do_action $action
			continue
		} elseif {[lsearch -bool -command $checker $value $var]} {
			set do_action $action
			break
		}
	}

	rename $checker ""

	if {[info exists do_action]} {
		set rc [catch [list uplevel 1 $do_action] result]
		return -code $rc $result
	}
}

# Optional argument is a glob pattern
proc parray {arrayname {pattern *}} {
	upvar $arrayname a

	set max 0
	foreach name [array names a $pattern]] {
		if {[string length $name] > $max} {
			set max [string length $name]
		}
	}
	incr max [string length $arrayname]
	incr max 2
	foreach name [lsort [array names a $pattern]] {
		puts [format "%-${max}s = %s" $arrayname\($name\) $a($name)]
	}
}

# Sort of replacement for $::errorInfo
# Usage: errorInfo error ?stacktrace?
proc errorInfo {error {stacktrace ""}} {
	if {$stacktrace eq ""} {
		set stacktrace [info stacktrace]
	}
	set result "Runtime Error: $error"
	foreach {l f p} [lreverse $stacktrace] {
		append result \n
		if {$p ne ""} {
			append result "in procedure '$p' "
			if {$f ne ""} {
				append result "called "
			}
		}
		if {$f ne ""} {
			append result "at file \"$f\", line $l"
		}
	}
	if {[info exists f] && $f ne ""} {
		return "$f:$l: $result"
	}
	return $result
}

set ::tcl_platform(platform) unix
