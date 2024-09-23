# Loads some Tcl-compatible features.
# I/O commands, parray, open |..., errorInfo, ::env
# try, throw, file copy, file delete -force
#
# (c) 2008 Steve Bennett <steveb@workware.net.au>


# Set up the ::env array
set env [env]

# Provide Tcl-compatible I/O commands
if {[exists -command stdout]} {
	# Tcl-compatible I/O commands
	foreach p {gets flush close eof seek tell} {
		proc $p {chan args} {p} {
			tailcall $chan $p {*}$args
		}
	}
	unset p

	# puts is complicated by -nonewline
	#
	proc puts {{-nonewline {}} {chan stdout} msg} {
		if {${-nonewline} ni {-nonewline {}}} {
			tailcall ${-nonewline} puts $msg
		}
		tailcall $chan puts {*}${-nonewline} $msg
	}

	# read is complicated by -nonewline
	#
	# read chan ?maxchars?
	# read -nonewline chan
	proc read {{-nonewline {}} chan} {
		if {${-nonewline} ni {-nonewline {}}} {
			tailcall ${-nonewline} read {*}${chan}
		}
		tailcall $chan read {*}${-nonewline}
	}

	proc fconfigure {f args} {
		foreach {n v} $args {
			switch -glob -- $n {
				-bl* {
					$f ndelay $(!$v)
				}
				-bu* {
					$f buffering $v
				}
				-tr* {
					# Just ignore -translation
				}
				default {
					return -code error "fconfigure: unknown option $n"
				}
			}
		}
	}
}

# fileevent isn't needed in Jim, but provide it for compatibility
proc fileevent {args} {
	tailcall {*}$args
}

# Second, optional argument is a glob pattern
# Third, optional argument is a "putter" function
proc parray {arrayname {pattern *} {puts puts}} {
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
		$puts [format "%-${max}s = %s" $arrayname\($name\) $a($name)]
	}
}

# Implements 'file copy' - single file mode only
proc {file copy} {{force {}} source target} {
	try {
		if {$force ni {{} -force}} {
			error "bad option \"$force\": should be -force"
		}

		set in [open $source rb]

		if {[file exists $target]} {
			if {$force eq ""} {
				error "error copying \"$source\" to \"$target\": file already exists"
			}
			# If source and target are the same, nothing to do
			if {$source eq $target} {
				return
			}
			# Hard linked, or case-insensitive filesystem
			# Note: mingw returns ino=0 for every file :-(
			file stat $source ss
			file stat $target ts
			if {$ss(dev) == $ts(dev) && $ss(ino) == $ts(ino) && $ss(ino)} {
				return
			}
		}
		set out [open $target wb]
		$in copyto $out
		$out close
	} on error {msg opts} {
		incr opts(-level)
		return {*}$opts $msg
	} finally {
		catch {$in close}
	}
}

# 'open "|..." ?mode?" will invoke this wrapper around exec/pipe
# Note that we return a lambda that also provides the 'pid' command
proc popen {cmd {mode r}} {
	lassign [pipe] r w
	try {
		if {[string match "w*" $mode]} {
			lappend cmd <@$r &
			set pids [exec {*}$cmd]
			$r close
			set f $w
		} else {
			lappend cmd >@$w &
			set pids [exec {*}$cmd]
			$w close
			set f $r
		}
		lambda {cmd args} {f pids} {
			if {$cmd eq "pid"} {
				return $pids
			}
			if {$cmd eq "close"} {
				$f close
				# And wait for the child processes to complete
				set retopts {}
				foreach p $pids {
					lassign [wait $p] status - rc
					if {$status eq "CHILDSTATUS"} {
						if {$rc == 0} {
							continue
						}
						set msg "child process exited abnormally"
					} else {
						set msg "child killed: received signal"
					}
					set retopts [list -code error -errorcode [list $status $p $rc] $msg]
				}
				return {*}$retopts
			}
			tailcall $f $cmd {*}$args
		}
	} on error {error opts} {
		$r close
		$w close
		error $error
	}
}

# A wrapper around 'pid' that can return the pids for 'popen'
local proc pid {{channelId {}}} {
	if {$channelId eq ""} {
		tailcall upcall pid
	}
	if {[catch {$channelId tell}]} {
		return -code error "can not find channel named \"$channelId\""
	}
	if {[catch {$channelId pid} pids]} {
		return ""
	}
	return $pids
}

# Generates an exception with the given code (ok, error, etc. or an integer)
# and the given message
proc throw {code {msg ""}} {
	return -code $code $msg
}

# Helper for "file delete -force"
proc {file delete force} {path} {
	foreach e [readdir $path] {
		file delete -force $path/$e
	}
	file delete $path
}
