# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Loads a Tcl6-compatible environment plus some newer features,
# including stdio, array, file, clock, glob, regexp, regsub, lsearch, case, ::env

package provide tcl6 1.0

package require stdio

# Extremely simple autoload approach
set autoload {glob glob array array}

proc unknown {cmd args} {
	if {[info exists ::autoload($cmd)]} {
		package require $::autoload($cmd)
		return [uplevel 1 $cmd $args]
	}
	error "invalid command name \"$cmd\""
}

# Set up the ::env array
set env [env]

# Very basic lsearch -exact with no options
proc lsearch {list value} {
	set i 0
	foreach elem $list {
		if {$elem eq $value} {
			return $i
		}
		incr i
	}
	return -1
}

# Internal function to match a value agains a list of patterns
proc _case_search_patterns {patterns value} {
	set i 0
	foreach pattern $patterns {
		if {[string match $pattern $value]} {
			return $i
		}
		incr i
	}
	return -1
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

	#puts "looking for $var in '$args'"
	foreach {value action} $args {
		if {$value eq "default"} {
			set do_action $action
			continue
		} else {
			if {[_case_search_patterns $value $var] >= 0} {
				set do_action $action
				break
			}
		}
	}

	if {[info exists do_action]} {
		return [uplevel 1 $do_action]
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
		puts [format "%-${max}s = $a($name)" $arrayname\($name\)]
    }
}

set ::tcl_platform(platform) unix
