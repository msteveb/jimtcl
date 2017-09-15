# Simple example of how the history extension
# can be used to provide line editing and history

# Build jimsh with the history extension and enable line editing (the default)
# ./configure --with-ext=history

package require history

set histfile [env HOME]/.jtclsh
history load $histfile
# Use the standard Tcl autocompletion
history completion tcl::autocomplete
set prefix ""
while {1} {
	# Read a complete line (script)
	set prompt "${prefix}jim> "
	set cmd {}
	while {1} {
		if {[history getline $prompt line] < 0} {
			exit 0
		}
		if {$cmd ne ""} {
			append cmd \n
		}
		append cmd $line
		if {[info complete $cmd char]} {
			break
		}
		set prompt "$char> "
	}

	if {$cmd eq "h"} {
		history show
		continue
	}

	# Don't bother adding single char commands to the history
	if {[string length $cmd] > 1} {
		history add $cmd
		history save $histfile
	}

	# Evaluate the script and display the error
	try {
		set result [eval $cmd]
		set prefix ""
	} on {error return break continue signal} {result opts} {
		set rcname [info returncodes $opts(-code)]
		if {$rcname eq "ok" } {
			# Note: return set -code to 0
			set rcname return
		} elseif {$rcname eq "error"} {
			set result [errorInfo $result]
		}
		set prefix "\[$rcname\] "
	}
	if {$result ne {}} {
		puts $result
	}
}
