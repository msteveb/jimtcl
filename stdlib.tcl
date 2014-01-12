# Implements script-based standard commands for Jim Tcl

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

# Returns a live stack trace as a list of proc filename line ...
# with 3 entries for each stack frame (proc),
# (deepest level first)
proc stacktrace {{skip 0}} {
	set trace {}
	incr skip
	foreach level [range $skip [info level]] {
		lappend trace {*}[info frame -$level]
	}
	return $trace
}

# Returns a human-readable version of a stack trace
proc stackdump {stacktrace} {
	set lines {}
	foreach {l f p} [lreverse $stacktrace] {
		set line {}
		if {$p ne ""} {
			append line "in procedure '$p' "
			if {$f ne ""} {
				append line "called "
			}
		}
		if {$f ne ""} {
			append line "at file \"$f\", line $l"
		}
		if {$line ne ""} {
			lappend lines $line
		}
	}
	join $lines \n
}

# Sort of replacement for $::errorInfo
# Usage: errorInfo error ?stacktrace?
proc errorInfo {msg {stacktrace ""}} {
	if {$stacktrace eq ""} {
		# By default add the stack backtrace and the live stacktrace
		set stacktrace [info stacktrace]
		# omit the procedure 'errorInfo' from the stack
		lappend stacktrace {*}[stacktrace 1]
	}
	lassign $stacktrace p f l
	if {$f ne ""} {
		set result "$f:$l: Error: "
	}
	append result "$msg\n"
	append result [stackdump $stacktrace]

	# Remove the trailing newline
	string trim $result
}

# Needs to be set up by the container app (e.g. jimsh)
# Returns the empty string if unknown
proc {info nameofexecutable} {} {
	if {[exists ::jim::exe]} {
		return $::jim::exe
	}
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
