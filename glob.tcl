# Implements a mostly Tcl-compatible glob command based on readdir
#
# (c) 2008 Steve Bennett <steveb@workware.net.au>
# (c) 2012 Alexander Shpilkin <ashpilkin@gmail.com>
#
# See LICENCE in this directory for licensing.

package require readdir

# Return a list of all entries in $dir that match the pattern.
proc glob.globdir {dir pattern} {
	set result {}
	set files [readdir $dir]
	lappend files . ..

	foreach name $files {
		if {[string match $pattern $name]} {
			# Starting dots match only explicitly
			if {[string index $name 0] eq "." && [string index $pattern 0] ne "."} {
				continue
			}
			lappend result $name
		}
	}

	return $result
}

# Return the list of patterns resulting from expanding any braced
# alternatives inside the given pattern, prepending the unprocessed
# part of the pattern. Does _not_ handle escaped braces or commas.
proc glob.explode {pattern} {
	set oldexp {}
	set newexp {""}

	while 1 {
		set oldexp $newexp
		set newexp {}
		set ob [string first \{ $pattern]
		set cb [string first \} $pattern]

		if {$ob < $cb && $ob != -1} {
			set mid [string range $pattern 0 $ob-1]
			set subexp [lassign [glob.explode [string range $pattern $ob+1 end]] pattern]
			if {$pattern eq ""} {
				error "unmatched open brace in glob pattern"
			}
			set pattern [string range $pattern 1 end]

			foreach subs $subexp {
				foreach sub [split $subs ,] {
					foreach old $oldexp {
						lappend newexp $old$mid$sub
					}
				}
			}
		} elseif {$cb != -1} {
			set suf  [string range $pattern 0 $cb-1]
			set rest [string range $pattern $cb end]
			break
		} else {
			set suf  $pattern
			set rest ""
			break
		}
	}

	foreach old $oldexp {
		lappend newexp $old$suf
	}
	linsert $newexp 0 $rest
}

# Core glob implementation. Returns a list of files/directories inside
# base matching pattern, in {realname name} pairs.
proc glob.glob {base pattern} {
	set dir [file dirname $pattern]
	if {$pattern eq $dir || $pattern eq ""} {
		return [list [file join $base $dir] $pattern]
	} elseif {$pattern eq [file tail $pattern]} {
		set dir ""
	}

	# Recursively expand the parent directory
	set dirlist [glob.glob $base $dir]
	set pattern [file tail $pattern]

	# Collect the files/directories
	set result {}
	foreach {realdir dir} $dirlist {
		if {![file isdir $realdir]} {
			continue
		}
		if {[string index $dir end] ne "/" && $dir ne ""} {
			append dir /
		}
		foreach name [glob.globdir $realdir $pattern] {
			lappend result [file join $realdir $name] $dir$name
		}
	}
	return $result
}

# Implements the Tcl glob command
#
# Usage: glob ?-nocomplain? ?-directory dir? ?--? pattern ...
#
# Patterns use 'string match' (glob) pattern matching for each
# directory level, plus support for braced alternations.
#
# e.g. glob {te[a-e]*/*.{c,tcl}}
#
# Note: files starting with . will only be returned if matching component
#       of the pattern starts with .
proc glob {args} {
	set nocomplain 0
	set base ""

	set n 0
	foreach arg $args {
		if {[info exists param]} {
			set $param $arg
			unset param
			incr n
			continue
		}
		switch -glob -- $arg {
			-d* {
				set switch $arg
				set param base
			}
			-n* {
				set nocomplain 1
			}
			-t* {
				# Ignored for Tcl compatibility
			}

			-* {
				return -code error "bad option \"$switch\": must be -directory, -nocomplain, -tails, or --"
			}
			-- {
				incr n
				break
			}
			*  {
				break
			}
		}
		incr n
	}
	if {[info exists param]} {
		return -code error "missing argument to \"$switch\""
	}
	if {[llength $args] <= $n} {
		return -code error "wrong # args: should be \"glob ?options? pattern ?pattern ...?\""
	}

	set args [lrange $args $n end]

	set result {}
	foreach pattern $args {
		set pattern [string map {
			\\\\ \x01 \\\{ \x02 \\\} \x03 \\, \x04
		} $pattern]
		set patexps [lassign [glob.explode $pattern] rest]
		if {$rest ne ""} {
			return -code error "unmatched close brace in glob pattern"
		}
		foreach patexp $patexps {
			set patexp [string map {
				\x01 \\\\ \x02 \{ \x03 \} \x04 ,
			} $patexp]
			foreach {realname name} [glob.glob $base $patexp] {
				lappend result $name
			}
		}
	}

	if {!$nocomplain && [llength $result] == 0} {
		return -code error "no files matched glob patterns"
	}

	return $result
}
