# A simplified version of Tcl expect using a pseudo-tty pair
# This could be turned into a standard module, but for now
# it is just used in the test suite

# Example usage:
#
# set p [expect::spawn {cmd pipeline}]
#
# $p timeout 5
# $p send "a command\r"
# $p expect {
#	ab.*c {
#		script
#	}
#	d[a-z] {
#		script
#	}
#   EOF { ... }
#   TIMEOUT { ... }
# }
#
# [$p before] returns data before the match
# [$p after] returns data that matches the pattern
# [$p buf] returns any data after the match that has been read
# $p close
#
# $p tty ?...?
# $p kill ?SIGNAL?

proc expect::spawn {cmd} {
	lassign [socket pty] m s
	# By default, turn off echo so that we can see just the output, not the input
	$m tty echo 0
	$m buffering none
	try {
		lappend cmd <@$s >@$s &
		set pids [exec {*}$cmd]
		$s close
		# Create a unique global variable for vwait
		set donevar ::[ref "" expect]
		set $donevar 0
		set matchinfo {
			buf {}
		}

		return [namespace current]::[lambda {cmd args} {m pids {timeout 30} donevar matchinfo {debug 0}} {
			#puts "expect::spawn cmd=$cmd, matchinfo=$matchinfo"
			# Find our own name
			set self [lindex [info level 0] 0]

			switch -exact -- $cmd {
				dputs {
					if {$debug} {
						set escapes {13 \\r 10 \\n 9 \\t 92 \\\\}
						lassign $args str
						# convert non-printable chars to printable
						set formatted {}
						binary scan $str cu* chars
						foreach c $chars {
							if {[exists escapes($c)]} {
								append formatted $escapes($c)
							} elseif {$c < 32} {
								append formatted [format \\x%02x $c]
							} elseif {$c > 127} {
								append formatted [format \\u%04x $c]
							} else {
								append formatted [format %c $c]
							}
						}
						puts $formatted
					}
				}
				kill {
					# kill the process with the given signal
					foreach i $pids {
						kill {*}$args $i
					}
				}
				pid {
					# return the process pids
					return $pids
				}
				getfd - tty {
					# pass through to the pty file descriptor
					tailcall $m $cmd {*}$args
				}
				close {
					# close the file descriptor, wait for the child process to complete
					# and return the result
					$m close
					set retopts {}
					foreach p $pids {
						lassign [wait $p] status - rc
						if {$status eq "CHILDSTATUS"} {
							# Don't treat a non-zero return code as fatal here
							if {[llength $retopts] <= 1} {
								set retopts $rc
							}
							continue
						} else {
							set msg "child killed: received signal"
						}
						set retopts [list -code error -errorcode [list $status $p $rc] $msg]
					}
					rename $self ""

					return {*}$retopts
				}
				timeout - debug {
					# set or return the variable
					if {[llength $args]} {
						set $cmd [lindex $args 0]
					} else {
						return [set $cmd]
					}
				}
				send {
					$self dputs ">>> [lindex $args 0]"
					# send to the process
					$m puts -nonewline [lindex $args 0]
					$m flush
				}
				before - after - buf {
					# return the before, after and remaining data
					return $matchinfo($cmd)
				}
				handle {
					# Internal use only
					set args [lassign $args type]
					switch -- $type {
						timeout {
							$self dputs "\[TIMEOUT patterns=$matchinfo(patterns) buf=$matchinfo(buf)\]"
							# a timeout occurred
							set matchinfo(before) $matchinfo(buf)
							set matchinfo(buf) {}
							set matchinfo(matched_pattern) TIMEOUT
							incr $donevar
							return 1
						}
						eof {
							$self dputs "\[EOF\]"
							# EOF was reached
							set matchinfo(before) $matchinfo(buf)
							set matchinfo(buf) {}
							set matchinfo(matched_pattern) EOF
							incr $donevar
							return 1
						}
						data {
							# data was received
							lassign $args data
							$self dputs "<<< $data"
							append matchinfo(buf) $data
							foreach pattern $matchinfo(patterns) {
								set result [regexp -inline -indices $pattern $matchinfo(buf)]
								if {[llength $result]} {
									$self dputs "MATCH=\[$pattern\]"
									lassign [lindex $result 0] start end
									set matchinfo(before) [string range $matchinfo(buf) 0 $start-1]
									set matchinfo(after) [string range $matchinfo(buf) $start $end]
									set matchinfo(buf) [string range $matchinfo(buf) $end+1 end]

									# Got a match, stop
									set matchinfo(matched_pattern) $pattern
									incr $donevar
									return 1
								}
							}
						}
					}
					return 0
				}
				expect {
					# Takes a list of regex-pattern, script, ... where the last script can be missing
					if {[llength $args] % 2 == 1} {
						lappend args {}
					}

					# Stash all the state in the matchinfo dict
					# Keep matchinfo(buf)
					array set matchinfo {
						before {}
						after {}
						patterns {}
						matched_pattern {}
					}

					foreach {pattern script} $args {
						lappend matchinfo(patterns) $pattern
					}

					# Handle the case where there is buffered data
					# that matches the pattern
					if {[$self handle data {}] == 0} {
						$m readable [namespace current]::[lambda {} {m self} {
							$m ndelay 1
							try {
								set buf [$m read]
								if {$buf eq ""} {
									$self handle eof "EOF"
								} else {
									$self handle data $buf
								}
							} on error msg {
								$self handle eof $msg
							}
							$m ndelay 0
						}]
						set matchinfo(afterid) [after $($timeout * 1e3) [list $self handle timeout]]

						vwait $donevar

						after cancel $matchinfo(afterid)
					}

					# Now invoke the matching script
					if {[dict exists $args $matchinfo(matched_pattern)]} {
						uplevel 1 [dict get $args $matchinfo(matched_pattern)]
					}
					# And return the data that matched the pattern
					# (is $matchinfo(before) more generally useful?)
					return $matchinfo(after)
				}
			}
		}]
	} on error {error opts} {
		catch {$m close}
		catch {$s close}
		return -code error $error
	}
}
