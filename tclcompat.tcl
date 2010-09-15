# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Loads some Tcl-compatible features.
# case, lassign, parray, errorInfo, ::tcl_platform, ::env

package provide tclcompat 1.0

# Set up the ::env array
set env [env]

# Tcl 8.5 lassign
proc lassign {list args} {
	# in case the list is empty...
	lappend list {}
	uplevel 1 [list foreach $args $list break]
	lrange $list [llength $args] end-1
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
		return -code error "extra case pattern with no body"
	}

	# Internal function to match a value agains a list of patterns
	local proc case.checker {value pattern} {
		string match $pattern $value
	}

	foreach {value action} $args {
		if {$value eq "default"} {
			set do_action $action
			continue
		} elseif {[lsearch -bool -command case.checker $value $var]} {
			set do_action $action
			break
		}
	}

	if {[info exists do_action]} {
		set rc [catch [list uplevel 1 $do_action] result opts]
		if {$rc} {
			incr opts(-level)
		}
		return {*}$opts $result
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
	lassign $stacktrace p f l
	if {$f ne ""} {
		set result "$f:$l "
	}
	append result "Runtime Error: $error\n"
	append result [stackdump $stacktrace]
}

proc {info nameofexecutable} {} {
	if {[info exists ::jim_argv0]} {
		if {[string first "/" $::jim_argv0] >= 0} {
			return $::jim_argv0
		}
		foreach path [split [env PATH ""] :] {
			set exec [file join $path $::jim_argv0]
			if {[file executable $exec]} {
				return $exec
			}
		}
	}
	return ""
}

# Implements 'file copy' - single file mode only
proc {file copy} {{force {}} source target} {
	try {
		if {$force ni {{} -force}} {
			error "bad option \"$force\": should be -force"
		}

		set in [open $source]

		if {$force eq "" && [file exists $target]} {
			$in close
			error "error copying \"$source\" to \"$target\": file already exists"
		}
		set out [open $target w]
		bio copy $in $out
		$out close
	} on error {msg opts} {
		incr opts(-level)
		return {*}$opts $msg
	} finally {
		catch {$in close}
	}
}

# 'open "|..." ?mode?" will invoke this wrapper around exec/pipe
proc popen {cmd {mode r}} {
	lassign [socket pipe] r w
	try {
		if {[string match "w*" $mode]} {
			lappend cmd <@$r &
			exec {*}$cmd
			$r close
			return $w
		} else {
			lappend cmd >@$w &
			exec {*}$cmd
			$w close
			return $r
		}
	} on error {error opts} {
		$r close
		$w close
		error $error
	}
}

# try/on/finally conceptually similar to Tcl 8.6
#
# Usage: try ?catchopts? script ?onclause ...? ?finallyclause?
#
# Where:
#   onclause is:       on codes {?resultvar? ?optsvar?} script
#
#   codes is: a list of return codes (ok, error, etc. or integers), or * for any
#
#   finallyclause is:  finally script
#
#
# Where onclause is: on codes {?resultvar? ?optsvar?}
proc try {args} {
	set catchopts {}
	while {[string match -* [lindex $args 0]]} {
		set args [lassign $args opt]
		if {$opt eq "--"} {
			break
		}
		lappend catchopts $opt
	}
	if {[llength $args] == 0} {
		return -code error {wrong # args: should be "try ?options? script ?argument ...?"}
	}
	set args [lassign $args script]
	set code [catch -eval {*}$catchopts [list uplevel 1 $script] msg opts]

	set handled 0

	foreach {on codes vars script} $args {
		switch -- $on \
			on {
				if {!$handled && ($codes eq "*" || [info returncode $code] in $codes)} {
					lassign $vars msgvar optsvar
					if {$msgvar ne ""} {
						upvar $msgvar hmsg
						set hmsg $msg
					}
					if {$optsvar ne ""} {
						upvar $optsvar hopts
						set hopts $opts
					}
					# Override any body result
					set code [catch [list uplevel 1 $script] msg opts]
					incr handled
				}
			} \
			finally {
				set finalcode [catch [list uplevel 1 $codes] finalmsg finalopts]
				if {$finalcode} {
					# Override any body or handler result
					set code $finalcode
					set msg $finalmsg
					set opts $finalopts
				}
				break
			} \
			default {
				return -code error "try: expected 'on' or 'finally', got '$on'"
			}
	}

	if {$code} {
		incr opts(-level)
		return {*}$opts $msg
	}
	return $msg
}

# Generates an exception with the given code (ok, error, etc. or an integer)
# and the given message
proc throw {code {msg ""}} {
	return -code $code $msg
}

set ::tcl_platform(platform) unix
