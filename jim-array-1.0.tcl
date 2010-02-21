# (c) 2008 Steve Bennett <steveb@workware.net.au>
#
# Implements a Tcl-compatible array command based on dict
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

package provide array 1.0

proc array {subcmd arrayname args} {
	# $name is the name of the array in the caller's context
	upvar $arrayname name

	if {$subcmd eq "exists"} {
		return [info exists name]
	}

	if {![info exists name]} {
		set name [dict create]
	}

	switch $subcmd {
		set {
			# The argument should be a list, but we also
			# support name value pairs
			if {[llength $args] == 1} {
				set args [lindex $args 0]
			}
			foreach {key value} $args {
				dict set name $key $value
			}
			return $name
		}
		size {
			return [/ [llength $name] 2]
		}
	}

	# The remaining options take a pattern
	if {[llength $args] > 0} {
		set pattern [lindex $args 0]
	} else {
		set pattern *
	}

	switch $subcmd {
		names {
			set keys {}
			foreach {key value} $name {
				if {[string match $pattern $key]} {
					lappend keys $key
				}
			}
			return $keys
		}
		get {
			set list {}
			foreach {key value} $name {
				if {[string match $pattern $key]} {
					lappend list $key $value
				}
			}
			return $list
		}
		unset {
			foreach {key value} $name {
				if {[string match $pattern $key]} {
					dict unset name $key
				}
			}
			return
		}
	}

	# Tcl-compatible error message
	error "bad option \"$subcmd\": must be exists, get, names, set, size, or unset"
}
