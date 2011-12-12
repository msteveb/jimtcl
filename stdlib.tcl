# Creates an anonymous procedure
proc lambda {arglist args} {
	tailcall proc [ref {} function lambda.finalizer] $arglist {*}$args
}

proc lambda.finalizer {name val} {
	rename $name {}
}

# Like alias, but creates and returns an anonyous procedure
proc curry {args} {
	alias [ref {} function lambda.finalizer] {*}$args
}

# Returns the given argument.
# Useful with 'local' as follows:
#   proc a {} {...}
#   local function a
#
#   set x [lambda ...]
#   local function $x
#
proc function {value} {
	return $value
}

# Returns a list of proc filename line ...
# with 3 entries for each stack frame (proc),
# (deepest level first)
proc stacktrace {} {
	set trace {}
	foreach level [range 1 [info level]] {
		lassign [info frame -$level] p f l
		lappend trace $p $f $l
	}
	return $trace
}

# Returns a human-readable version of a stack trace
proc stackdump {stacktrace} {
	set result {}
	set count 0
	foreach {l f p} [lreverse $stacktrace] {
		if {$count} {
			append result \n
		}
		incr count
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
	return $result
}

# Sort of replacement for $::errorInfo
# Usage: errorInfo error ?stacktrace?
proc errorInfo {msg {stacktrace ""}} {
	if {$stacktrace eq ""} {
		set stacktrace [info stacktrace]
	}
	lassign $stacktrace p f l
	if {$f ne ""} {
		set result "Runtime Error: $f:$l: "
	}
	append result "$msg\n"
	append result [stackdump $stacktrace]

	# Remove the trailing newline
	string trim $result
}

# Finds the current executable by searching along the path
# Returns the empty string if not found.
proc {info nameofexecutable} {} {
	if {[info exists ::jim_argv0]} {
		if {[string match "*/*" $::jim_argv0]} {
			return [file join [pwd] $::jim_argv0]
		}
		foreach path [split [env PATH ""] $::tcl_platform(pathSeparator)] {
			set exec [file join [pwd] [string map {\\ /} $path] $::jim_argv0]
			if {[file executable $exec]} {
				return $exec
			}
		}
	}
	return ""
}

# Script-based implementation of 'dict with'
proc {dict with} {&dictVar {args key} script} {
	set keys {}
	foreach {n v} [dict get $dictVar {*}$key] {
		upvar $n var_$n
		set var_$n $v
		lappend keys $n
	}
	catch {uplevel 1 $script} msg opts
	if {[info exists dictVar] && ([llength $key] == 0 || [dict exists $dictVar {*}$key])} {
		foreach n $keys {
			if {[info exists var_$n]} {
				dict set dictVar {*}$key $n [set var_$n]
			} else {
				dict unset dictVar {*}$key $n
			}
		}
	}
	return {*}$opts $msg
}

# Script-based implementation of 'dict update'
proc {dict update} {&varName args script} {
	set keys {}
	foreach {n v} $args {
		upvar $v var_$v
		if {[dict exists $varName $n]} {
			set var_$v [dict get $varName $n]
		}
	}
	catch {uplevel 1 $script} msg opts
	if {[info exists varName]} {
		foreach {n v} $args {
			if {[info exists var_$v]} {
				dict set varName $n [set var_$v]
			} else {
				dict unset varName $n
			}
		}
	}
	return {*}$opts $msg
}

# Script-based implementation of 'dict merge'
# This won't get called in the trivial case of no args
proc {dict merge} {dict args} {
	foreach d $args {
		# Check for a valid dict
		dict size $d
		foreach {k v} $d {
			dict set dict $k $v
		}
	}
	return $dict
}

proc {dict replace} {dictionary {args {key value}}} {
	if {[llength ${key value}] % 2} {
		tailcall {dict replace}
	}
	tailcall dict merge $dictionary ${key value}
}

# Script-based implementation of 'dict lappend'
proc {dict lappend} {varName key {args value}} {
	upvar $varName dict
	if {[exists dict] && [dict exists $dict $key]} {
		set list [dict get $dict $key]
	}
	lappend list {*}$value
	dict set dict $key $list
}

# Script-based implementation of 'dict append'
proc {dict append} {varName key {args value}} {
	upvar $varName dict
	if {[exists dict] && [dict exists $dict $key]} {
		set str [dict get $dict $key]
	}
	append str {*}$value
	dict set dict $key $str
}

# Script-based implementation of 'dict incr'
proc {dict incr} {varName key {increment 1}} {
	upvar $varName dict
	if {[exists dict] && [dict exists $dict $key]} {
		set value [dict get $dict $key]
	}
	incr value $increment
	dict set dict $key $value
}

# Script-based implementation of 'dict remove'
proc {dict remove} {dictionary {args key}} {
	foreach k $key {
		dict unset dictionary $k
	}
	return $dictionary
}

# Script-based implementation of 'dict values'
proc {dict values} {dictionary {pattern *}} {
	dict keys [lreverse $dictionary] $pattern
}

# Script-based implementation of 'dict for'
proc {dict for} {vars dictionary script} {
	if {[llength $vars] != 2} {
		return -code error "must have exactly two variable names"
	}
	dict size $dictionary
	tailcall foreach $vars $dictionary $script
}
