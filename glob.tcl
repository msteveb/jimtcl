# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Implements a Tcl-compatible glob command based on readdir
#
# The FreeBSD license
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# The views and conclusions contained in the software and documentation
# are those of the authors and should not be interpreted as representing
# official policies, either expressed or implied, of the Jim Tcl Project.

package provide glob

# If $dir is a directory, return a list of all entries
# it contains which match $pattern
#
proc _glob_readdir_pattern {dir pattern} {
	set result {}

	# readdir doesn't return . or .., so simulate it here
	if {$pattern eq "." || $pattern eq ".."} {
		return $pattern
	}
	# Use -nocomplain here to return nothing if $dir is not a directory
	foreach name [readdir -nocomplain $dir] {
		if {[string match $pattern $name]} {
			lappend result $name
		}
	}

	return $result
}

# glob entries in directory $dir and pattern $rem
#
proc _glob_do {dir rem} {
	# Take one level from rem
	# Avoid regexp here
	set i [string first / $rem]
	if {$i < 0} {
		set pattern $rem
		set rempattern ""
	} else {
		set j $i
		incr j
		incr i -1
		set pattern [string range $rem 0 $i]
		set rempattern [string range $rem $j end]
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

	# Use readdir and select all files which match the pattern
	foreach f [_glob_readdir_pattern $globdir $pattern] {
		if {$rempattern eq ""} {
			# This is a terminal entry, so add it
			lappend result $dir$sep$f
		} else {
			# Expany any entries at this level and add them
			lappend result {expand}[_glob_do $dir$sep$f $rempattern]
		}
	}
	return $result
}

# Implements the Tcl glob command
#
# Usage: glob ?-nocomplain? pattern ...
#
# Patterns use string match pattern matching for each
# directory level.
#
# e.g. glob te[a-e]*/*.tcl
#
proc glob {args} {
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
			lappend result {expand}[_glob_do / [string range $pattern 1 end]]
		} else {
			lappend result {expand}[_glob_do "" $pattern]
		}
	}

	if {$nocomplain == 0 && [llength $result] == 0} {
		error "no files matched glob patterns"
	}

	return $result
}
