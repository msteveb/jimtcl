# Implements script-based standard commands for Jim Tcl

if {![exists -command ref]} {
	# No support for references, so create a poor-man's reference just good enough for lambda
	proc ref {args} {{count 0}} {
		format %08x [incr count]
	}
}

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

# Returns a human-readable version of a stack trace
proc stackdump {stacktrace} {
	set lines {}
	lappend lines "Traceback (most recent call last):"
	foreach {cmd l f p} [lreverse $stacktrace] {
		set line {}
		if {$f ne ""} {
			append line "  File \"$f\", line $l"
		}
		if {$p ne ""} {
			append line ", in $p"
		}
		if {$line ne ""} {
			lappend lines $line
			if {$cmd ne ""} {
				set nl [string first \n $cmd 1]
				if {$nl >= 0} {
					set cmd [string range $cmd 0 $nl-1]...
				}
				lappend lines "    $cmd"
			}
		}
	}
	if {[llength $lines] > 1} {
		return [join $lines \n]
	}
}

# Add the given script to $jim::defer, to be evaluated when the current
# procedure exits
proc defer {script} {
	upvar jim::defer v
	lappend v $script
}

# Sort of replacement for $::errorInfo
# Usage: errorInfo error ?stacktrace?
proc errorInfo {msg {stacktrace ""}} {
	if {$stacktrace eq ""} {
		# By default add the stack backtrace and the live stacktrace
		set stacktrace [info stacktrace]
	}
	lassign $stacktrace p f l cmd
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

# Script-based implementation of 'dict for'
proc {dict for} {vars dictionary script} {
	if {[llength $vars] != 2} {
		return -code error "must have exactly two variable names"
	}
	dict size $dictionary
	tailcall foreach $vars $dictionary $script
}
